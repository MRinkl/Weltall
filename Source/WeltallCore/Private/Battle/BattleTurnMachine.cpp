#include "Battle/BattleTurnMachine.h"

#include "Battle/BattleAction.h"
#include "Battle/BattleAi.h"
#include "Battle/BattleContext.h"
#include "Battle/BattleEnd.h"
#include "Battle/BattleEvent.h"
#include "Battle/BattleParty.h"
#include "Battle/BattleRules.h"
#include "Battle/BattleTurn.h"
#include "Battle/BattleWaits.h"
#include "Game/GameState.h"

namespace Weltall
{
	namespace
	{
		/** Stopped, or the flag beside it: the slot does nothing this turn. */
		constexpr uint16 CannotAct = 0x2080;
		constexpr uint16 Asleep = 0x1000;
		constexpr uint16 Confused = 0x2000;
		constexpr uint8 HiddenEnemy = 0x80;
		constexpr uint16 NoNameBox = 0x0400;
		constexpr uint8 NameFrames = 3;
		constexpr uint8 HoldFrames = 59;
		constexpr uint8 NoPress = 8;

		enum ETurnPc : uint8
		{
			TurnDone,
			PartyInput,
			PartyAfterInput,
			PartyAfterMessage,
			PartyAfterHide,
			PartyAfterHook,
			ReactionNext,
			ReactionAfterActions,
			ReactionAfterHook,
			EnemyAfterActions,
			EnemyAfterHide,
			EnemyAfterMessage,
			EnemyAfterSecondHide,
			TurnEnd,
			EndAfterRefresh,
			EndAfterReady,
			EndAfterDrains,
			EndAfterSecondRefresh,
			EndAfterPoses,
			MessageShowing,
			MessageHolding,
			MessageClosed,
			ActionsBegin,
			ActionsPrepared,
			ActionsStarted,
			ActionsPlaying,
			HookRunning,
			HideFrame,
			HideFrameDone,
			ShowWaiting,
		};

		/** Whether a step waits for the next frame. */
		enum class EFlow : uint8
		{
			Go,
			Frame,
			Done,
		};

		void Cue(FBattleContext& Context, EBattleTurnCue Kind, uint8 Arg)
		{
			if (Context.Show != nullptr)
			{
				Context.Show->PlayTurnCue(Kind, Arg);
			}
		}

		FBattleTurnWork& Turn(FBattleContext& Context)
		{
			return Context.Turn;
		}

		/** A shared step begins; it goes on at Then when it returns. */
		void Call(FBattleContext& Context, uint8 Step, uint8 Then)
		{
			FBattleTurnWork& Work = Turn(Context);
			if (Work.Depth >= Work.Returns.size())
			{
				Context.Fault = EBattleFault::IndexOutsideTable;
				return;
			}
			Work.Returns[Work.Depth++] = Then;
			Work.Pc = Step;
		}

		void Return(FBattleContext& Context)
		{
			FBattleTurnWork& Work = Turn(Context);
			Work.Pc = Work.Returns[--Work.Depth];
		}

		bool HasActionName(const FBattleContext& Context)
		{
			return Context.Action.Message != 0 && (Context.Action.Targets & Context.Action.KnockedOut) == 0;
		}

		/** The action's name, when it left one and none of its targets fell to it: 3 frames, then its box up to 59 frames or a press. */
		void CallActionName(FBattleContext& Context, uint8 Then)
		{
			if (!HasActionName(Context))
			{
				Turn(Context).Pc = Then;
				return;
			}
			Cue(Context, EBattleTurnCue::ActionName, Context.Action.Message);
			Turn(Context).Frames = NameFrames;
			Call(Context, MessageShowing, Then);
		}

		/** Frames while the show plays what the call just made began (the show's own frames, which the game takes inside the call). */
		void CallShowWait(FBattleContext& Context, uint8 Then)
		{
			Call(Context, ShowWaiting, Then);
		}

		/** The message boxes hidden, and a frame. */
		void CallHideBoxes(FBattleContext& Context, uint8 Then)
		{
			Cue(Context, EBattleTurnCue::HideMessageBoxes, 0);
			Call(Context, HideFrame, Then);
		}

		/** The battle event's hook: its scripts run to their return, when the battle has an event. */
		void CallEventHook(FBattleContext& Context, uint8 Then)
		{
			if (!Context.bEventBattle)
			{
				Turn(Context).Pc = Then;
				return;
			}
			BeginBattleEventRun(Context);
			Call(Context, HookRunning, Then);
		}

		/** An enemy's actions: begun, a frame, carried out, then the frames the show takes to play them. */
		void CallEnemyActions(FBattleContext& Context, uint8 Slot, uint8 Then)
		{
			Turn(Context).Performer = Slot;
			Call(Context, ActionsBegin, Then);
		}

		void RefreshPanels(FBattleContext& Context)
		{
			for (std::size_t Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
			{
				Context.PanelRefresh[Slot] = Context.ChangedSlots[Slot];
			}
		}

		bool CanAct(const FBattleCombatant& Actor)
		{
			return (Actor.Status & CannotAct) == 0 && (Actor.Mental & Asleep) == 0;
		}

		/** The turn's steps from where it stopped to its next frame (Frame) or its end (Done). */
		EFlow Run(FBattleContext& Context, uint8 Event)
		{
			FBattleTurnWork& Work = Turn(Context);
			while (Context.Fault == EBattleFault::None)
			{
				switch (Work.Pc)
				{
				case TurnDone:
					return EFlow::Done;
				case PartyInput:
					if (ResumeBattlePartyTurn(Context, Event))
					{
						return EFlow::Frame;
					}
					Work.Pc = PartyAfterInput;
					break;
				case PartyAfterInput:
					if (Context.Party.bActed)
					{
						++Context.GameState->BattleActingTurns;
					}
					if (Context.Show != nullptr)
					{
						Context.Show->PlayMenuCue(EBattleMenuCue::HideApTicks, Context.TurnActor, 0);
						Context.Show->SetTargetMarkers(0);
					}
					CallActionName(Context, PartyAfterMessage);
					break;
				case PartyAfterMessage:
					CallHideBoxes(Context, PartyAfterHide);
					break;
				case PartyAfterHide:
					CallEventHook(Context, PartyAfterHook);
					break;
				case PartyAfterHook:
					for (uint8 Enemy = 0; Enemy < FBattleContext::EnemySlotCount; ++Enemy)
					{
						if (((Context.Action.Targets >> (Enemy + FBattleContext::PartySlotCount)) & 1) != 0)
						{
							Context.Targeted[Enemy] = true;
						}
					}
					BeginBattleReactions(Context);
					Work.Reactor = 0;
					Work.Pc = ReactionNext;
					break;
				case ReactionNext:
					while (Work.Reactor < FBattleContext::EnemySlotCount && !RunBattleReactionScript(Context, Work.Reactor))
					{
						++Work.Reactor;
					}
					if (Context.Fault != EBattleFault::None)
					{
						return EFlow::Done;
					}
					if (Work.Reactor >= FBattleContext::EnemySlotCount)
					{
						EndBattleReactions(Context);
						Work.Pc = TurnEnd;
						break;
					}
					Context.bActionsPlayed = false;
					CallEnemyActions(Context, static_cast<uint8>(FBattleContext::PartySlotCount + Work.Reactor), ReactionAfterActions);
					break;
				case ReactionAfterActions:
					CallEventHook(Context, ReactionAfterHook);
					break;
				case ReactionAfterHook:
					++Work.Reactor;
					Work.Pc = ReactionNext;
					break;
				case EnemyAfterActions:
					CallHideBoxes(Context, EnemyAfterHide);
					break;
				case EnemyAfterHide:
					CallActionName(Context, EnemyAfterMessage);
					break;
				case EnemyAfterMessage:
					CallHideBoxes(Context, EnemyAfterSecondHide);
					break;
				case EnemyAfterSecondHide:
					CallEventHook(Context, TurnEnd);
					break;
				case TurnEnd:
					RefreshPanels(Context);
					Work.Pc = EndAfterRefresh;
					return EFlow::Frame;
				case EndAfterRefresh:
				{
					ResetRevivedWaits(Context);
					Work.bActorGlow = false;
					if (Context.Show != nullptr)
					{
						Context.Show->PlayMenuCue(EBattleMenuCue::ActorFlash, Context.TurnActor, 0);
					}
					for (uint8 Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
					{
						AimBattleSlot(Context, Slot);
					}
					Cue(Context, EBattleTurnCue::ReadyNext, FindNextReadySlot(Context, Context.TurnActor));
					CallShowWait(Context, EndAfterReady);
					break;
				}
				case EndAfterReady:
					for (std::size_t Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
					{
						Context.ChangedSlots[Slot] = 0;
					}
					CheckBattleEnd(Context);
					if (Context.End == BattleEnd::Running)
					{
						ApplyTurnDrains(Context, Context.TurnActor);
					}
					CallShowWait(Context, EndAfterDrains);
					break;
				case EndAfterDrains:
					CheckBattleEnd(Context);
					RefreshPanels(Context);
					Work.Pc = EndAfterSecondRefresh;
					return EFlow::Frame;
				case EndAfterSecondRefresh:
					Cue(Context, EBattleTurnCue::RefreshPoses, 0);
					CallShowWait(Context, EndAfterPoses);
					break;
				case EndAfterPoses:
					RollActorWait(Context);
					Context.bWaitsRunning = true;
					Work.Pc = TurnDone;
					Work.bRunning = false;
					return EFlow::Done;
				case MessageShowing:
					if (Work.Frames > 0)
					{
						--Work.Frames;
						return EFlow::Frame;
					}
					Cue(Context, EBattleTurnCue::ActionNameBox, 1);
					Context.bActionsPlayed = false;
					Work.Frames = HoldFrames;
					Work.Pc = MessageHolding;
					return EFlow::Frame;
				case MessageHolding:
					if (Event == NoPress && --Work.Frames != 0)
					{
						return EFlow::Frame;
					}
					Context.bActionsPlayed = true;
					Cue(Context, EBattleTurnCue::ActionNameBox, 0);
					Work.Pc = MessageClosed;
					return EFlow::Frame;
				case MessageClosed:
					Return(Context);
					break;
				case ActionsBegin:
					PrepareBattleEnemyActions(Context, Work.Performer);
					Work.Pc = ActionsPrepared;
					return EFlow::Frame;
				case ActionsPrepared:
					BeginBattleEnemyPlayback(Context, Work.Performer);
					CallShowWait(Context, ActionsStarted);
					break;
				case ActionsStarted:
					RunBattleEnemyActions(Context, Work.Performer);
					Work.Pc = ActionsPlaying;
					break;
				case ActionsPlaying:
					if (!Context.bActionsPlayed)
					{
						return EFlow::Frame;
					}
					Return(Context);
					break;
				case HookRunning:
					if (StepBattleEvent(Context))
					{
						return EFlow::Frame;
					}
					Return(Context);
					break;
				case HideFrame:
					Work.Pc = HideFrameDone;
					return EFlow::Frame;
				case HideFrameDone:
					Return(Context);
					break;
				case ShowWaiting:
					if (Context.Show != nullptr && Context.Show->IsBlocking())
					{
						return EFlow::Frame;
					}
					Return(Context);
					break;
				default:
					return EFlow::Done;
				}
			}
			return EFlow::Done;
		}
	}

	void BeginBattleTurn(FBattleContext& Context, bool bScripted)
	{
		FBattleTurnWork& Work = Context.Turn;
		Work = FBattleTurnWork{};
		Work.bScripted = bScripted;
		Context.NextMessageBox = 1;
		Context.Action.Message = 0;
		if (Context.TurnActor == 0)
		{
			return;
		}
		ResetBattleTurnTotals(Context);
		for (std::size_t Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
		{
			Context.ChangedSlots[Slot] = 0;
		}
		Context.bWaitsRunning = false;
		Context.TurnActor = static_cast<uint8>(Context.TurnActor - 1);
		const uint8 Actor = Context.TurnActor;
		if (Actor >= FBattleContext::SlotCount)
		{
			Context.Fault = EBattleFault::SlotOutsideBattle;
			return;
		}
		Context.PerformanceIndex = 0;
		Context.bActionsPlayed = false;
		Context.Action.Info = {};
		Context.Action.InfoCommand = 0;
		Context.Action.Targets = 0;
		TickStatusDurations(Context, Actor);
		Work.bRunning = true;
		const FBattleCombatant& Member = Context.Combatants[Actor];
		if (FBattleContext::IsParty(Actor))
		{
			Context.Targeted = {};
			Work.bActorGlow = true;
			if (Context.Show != nullptr)
			{
				Context.Show->PlayMenuCue(EBattleMenuCue::ActorFlash, Actor, 1);
			}
			if (CanAct(Member))
			{
				if ((Member.Mental & Confused) != 0)
				{
					BeginBattleAutoTurn(Context, Actor);
				}
				else
				{
					BeginBattlePartyTurn(Context, Actor);
				}
				Work.Pc = Context.Party.Depth > 0 ? PartyInput : PartyAfterInput;
				if (Work.Pc == PartyInput)
				{
					return;
				}
			}
			else
			{
				CallActionName(Context, PartyAfterMessage);
			}
		}
		else
		{
			if (!bScripted)
			{
				RunBattleTurnScript(Context, Actor);
				if ((Context.Slots[Actor].Presence & HiddenEnemy) == 0 && (Member.Traits & NoNameBox) == 0 && Context.Show != nullptr)
				{
					Context.Show->ShowNameBox(Context.NameEntries[Actor]);
				}
			}
			if (CanAct(Context.Combatants[Actor]))
			{
				CallEnemyActions(Context, Actor, EnemyAfterActions);
			}
			else
			{
				CallHideBoxes(Context, EnemyAfterSecondHide);
			}
		}
		if (Run(Context, Context.Party.Event) == EFlow::Done)
		{
			Work.bRunning = false;
		}
	}

	bool StepBattleTurn(FBattleContext& Context, uint8 Event)
	{
		if (!Context.Turn.bRunning || Context.Fault != EBattleFault::None)
		{
			return false;
		}
		Context.Party.Event = Event;
		return Run(Context, Event) == EFlow::Frame;
	}
}
