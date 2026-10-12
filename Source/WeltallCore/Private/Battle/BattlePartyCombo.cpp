#include "Battle/BattleAction.h"
#include "Battle/BattlePartySteps.h"
#include "Battle/BattleRules.h"
#include "Battle/BattleSteps.h"
#include "Game/GameStateLayout.h"

namespace Weltall::BattleParty
{
	namespace
	{
		constexpr uint8 Empty = 0xFF;
		/** The menu's eighth cell, which accepts the queue. */
		constexpr uint8 AcceptCell = 7;
		constexpr uint8 Chosen = 1;
		constexpr uint8 Left = 2;
		constexpr uint8 ComboWindow = 3;
		constexpr uint8 FirstDeathblow = 7;
		constexpr uint8 FirstDeathblowAttack = 8;
		constexpr uint8 ReactionCode = 0xF3;
		constexpr uint16 ShowsReactions = 0x0800;
		/** The Combo command aims at one enemy the actor can reach. */
		constexpr uint16 OneEnemy = 0x1000;
		/** Its two windows close a frame each; then a frame passes, and one more for the list's work. */
		constexpr uint8 ComboWindows = 2;
		constexpr uint8 CloseFrames = 4;

		/** The frames a window growing 32 pixels a frame each way from nothing takes to reach its size. */
		constexpr uint8 GrowFrames(uint32 Width, uint32 Height)
		{
			const uint32 Across = (Width + 31) / 32;
			const uint32 Down = (Height + 31) / 32;
			return static_cast<uint8>(Across > Down ? Across : Down);
		}

		/** The menu waits until both its windows (290 by 76 and 232 by 44) have grown. */
		constexpr uint8 OpenFrames = GrowFrames(290, 76) > GrowFrames(232, 44) ? GrowFrames(290, 76) : GrowFrames(232, 44);

		enum EComboPc : uint8
		{
			ComboStart,
			ComboBuilt,
			ComboTop,
			ComboFrame,
			ComboAfterSelect,
			ComboSettle,
			ComboClosing,
			ComboClosed,
			ComboStarted,
			ComboWait,
		};

		bool HighBit(uint16 Word, uint8 Bit)
		{
			return Bit < 16 && (Word & (0x8000u >> Bit)) != 0;
		}

		void OpenCombo(FBattleContext& Context, uint8 Slot)
		{
			FBattlePartyInput& Party = Context.Party;
			FBattleComboMenu& Combo = Party.Combo;
			Party.bFirstWindowGrown = false;
			Combo.Cursor = 0;
			Combo.Queued = 0;
			Combo.Result = 0;
			Combo.Pool = Context.ApRecords[Slot].StoredAp;
			Party.bIconsShown = false;
			Combo.Offered.fill(Empty);
			Combo.Costs.fill(0);
			Party.ComboQueue.fill(Empty);
			Party.ComboCells.fill(Empty);
			const uint16 Learned = LearnedWord(Context, Context.PartyCharacters[Slot], GameStateLayout::Skills::Deathblows);
			uint8 Offered = 0;
			for (uint8 Deathblow = 0; Deathblow < FirstDeathblow; ++Deathblow)
			{
				if (HighBit(Learned, Deathblow))
				{
					Combo.Offered[Offered] = Deathblow;
					Combo.Costs[Offered] = BattleRules::CommandAt(Context, FBattleContext::PartyCommandIndex(Slot, FirstDeathblow + Deathblow)).ApCost;
					++Offered;
				}
			}
		}

		/** The frame's event in the menu: the cursor over the 2 by 4 cells, a Deathblow queued or taken back, the queue accepted. */
		EFlow ComboEvent(FBattleContext& Context, FBattlePartyFrame& Frame, uint8 Slot)
		{
			FBattlePartyInput& Party = Context.Party;
			FBattleComboMenu& Combo = Party.Combo;
			uint8 Cursor = Combo.Cursor;
			switch (Party.Event)
			{
			case BattleMenuEvents::Right:
				Cursor = Combo.Cursor + 1 > AcceptCell ? Combo.Cursor : static_cast<uint8>(Combo.Cursor + 1);
				break;
			case BattleMenuEvents::Down:
				Cursor = Combo.Cursor + 2 > AcceptCell ? Combo.Cursor : static_cast<uint8>(Combo.Cursor + 2);
				break;
			case BattleMenuEvents::Left:
				Cursor = Combo.Cursor != 0 ? static_cast<uint8>(Combo.Cursor - 1) : Combo.Cursor;
				break;
			case BattleMenuEvents::Up:
				Cursor = Combo.Cursor > 1 ? static_cast<uint8>(Combo.Cursor - 2) : Combo.Cursor;
				break;
			case BattleMenuEvents::Confirm:
				if (Combo.Cursor == AcceptCell)
				{
					if (Party.ComboQueue[0] == Empty)
					{
						Combo.Result = Left;
						break;
					}
					Party.ListWindow = 0;
					PlayMenuCue(Context, EBattleMenuCue::HideListWindows, Slot, static_cast<uint8>(EBattleMenuList::Combo));
					PlayMenuCue(Context, EBattleMenuCue::ShowArrows, Slot);
					Frame.Pc = ComboAfterSelect;
					return CallSelectTarget(Context, OneEnemy, Slot, true);
				}
				if (Combo.Queued != AcceptCell && Combo.Offered[Combo.Cursor] != Empty && Combo.Costs[Combo.Cursor] <= Combo.Pool)
				{
					const uint8 Cost = Combo.Costs[Combo.Cursor];
					Party.ComboQueue[Combo.Queued] = Combo.Offered[Combo.Cursor];
					Party.ComboCells[Combo.Queued] = Combo.Cursor;
					Combo.Paid[Combo.Queued] = Cost;
					Combo.Pool = static_cast<uint8>(Combo.Pool - Cost);
					++Combo.Queued;
				}
				break;
			case BattleMenuEvents::Cancel:
				if (Combo.Queued == 0)
				{
					Combo.Result = Left;
					break;
				}
				--Combo.Queued;
				Combo.Pool = static_cast<uint8>(Combo.Pool + Combo.Paid[Combo.Queued]);
				Party.ComboQueue[Combo.Queued] = Empty;
				Party.ComboCells[Combo.Queued] = Empty;
				break;
			case BattleMenuEvents::Weak:
			case BattleMenuEvents::Strong:
			case 9:
			case 10:
				Cursor = AcceptCell;
				break;
			default:
				break;
			}
			Combo.Cursor = Cursor;
			Frame.Pc = ComboSettle;
			return EFlow::Return;
		}

		/** The Combo command's Deathblows begin on the chosen target: its playback's start (the show then takes frames). */
		void BeginStoredCombo(FBattleContext& Context, uint8 Slot)
		{
			FBattlePartyInput& Party = Context.Party;
			Party.bMenuSounds = false;
			SetMarkers(Context, 0);
			Context.SlotTargets[Slot] = Context.Targeting.Target;
			BattleSteps::PlanApproachPath(Context, Slot, Context.SlotTargets[Slot]);
			Party.bStoredCombo = true;
			BeginAttack(Context, Slot);
		}

		/** The queued Deathblows, all on the chosen target, each computed, applied and queued in turn. */
		void RunStoredCombo(FBattleContext& Context, uint8 Slot)
		{
			FBattlePartyInput& Party = Context.Party;
			FinishAttackStart(Context);
			const uint8 Target = Context.SlotTargets[Slot];
			if (Target >= FBattleContext::SlotCount)
			{
				Context.Fault = EBattleFault::SlotOutsideBattle;
				return;
			}
			if ((Context.Combatants[Target].Traits & ShowsReactions) != 0)
			{
				QueueCode(Context, Slot, ReactionCode, Bit(Context, Target));
			}
			for (const uint8 Deathblow : Party.ComboQueue)
			{
				if (Deathblow == Empty || Context.Fault != EBattleFault::None)
				{
					continue;
				}
				Context.CurrentAttack = static_cast<uint8>(Deathblow + FirstDeathblowAttack);
				const uint8 Command = static_cast<uint8>(Context.CurrentAttack - 1);
				PerformBattleCommand(Context, Slot, Command, Bit(Context, Target), Command);
				FBattlePerformance* Record = CurrentRecord(Context);
				if (Context.Fault != EBattleFault::None || Record == nullptr)
				{
					return;
				}
				CommitBattleResults(Context, Context.PerformanceIndex);
				Record->Code = Command;
				Record->Actor = Slot;
				Record->Targets = Context.Action.Targets;
				++Context.PerformanceIndex;
			}
			QueueEnd(Context, Slot);
			BattleSteps::MoveToTargetGroup(Context, Slot, Target);
		}
	}

	EFlow RunComboMenu(FBattleContext& Context, FBattlePartyFrame& Frame)
	{
		FBattlePartyInput& Party = Context.Party;
		FBattleComboMenu& Combo = Party.Combo;
		const uint8 Slot = Party.Actor;
		for (;;)
		{
			switch (Frame.Pc)
			{
			case ComboStart:
				OpenCombo(Context, Slot);
				for (uint8 Window = 0; Window < ComboWindows; ++Window)
				{
					PlayMenuCue(Context, EBattleMenuCue::OpenListWindow, Slot, static_cast<uint8>(static_cast<uint8>(EBattleMenuList::Combo) * 16 + Window));
				}
				Frame.Pc = ComboBuilt;
				return CallFrames(Context, OpenFrames);
			case ComboBuilt:
				PlayMenuCue(Context, EBattleMenuCue::BuildComboList, Slot);
				Party.ListWindow = ComboWindow;
				PlayMenuCue(Context, EBattleMenuCue::InitArrows, Slot);
				Frame.Pc = ComboTop;
				break;
			case ComboTop:
				Party.bFirstWindowGrown = true;
				PlayMenuCue(Context, EBattleMenuCue::ComboListFrame, Slot);
				Frame.Pc = ComboFrame;
				return EFlow::Yield;
			case ComboFrame:
				if (ComboEvent(Context, Frame, Slot) == EFlow::Call)
				{
					return EFlow::Call;
				}
				break;
			case ComboAfterSelect:
				if (Party.Returned == 0)
				{
					PlayMenuCue(Context, EBattleMenuCue::HideArrows, Slot);
					PlayMenuCue(Context, EBattleMenuCue::ShowListWindows, Slot, static_cast<uint8>(EBattleMenuList::Combo));
					Party.ListWindow = ComboWindow;
					FrameCamera(Context, static_cast<uint16>(Bit(Context, Slot) | Bit(Context, Context.SlotTargets[Slot])));
					SetMarkers(Context, Bit(Context, Context.SlotTargets[Slot]));
				}
				else
				{
					Context.ApRecords[Slot].StoredAp = Combo.Pool;
					Combo.Result = Chosen;
				}
				Frame.Pc = ComboSettle;
				break;
			case ComboSettle:
				if (Combo.Result == 0)
				{
					Frame.Pc = ComboTop;
					break;
				}
				Party.ListWindow = 0;
				if (Combo.Result == Chosen)
				{
					PlayMenuCue(Context, EBattleMenuCue::ShowApNumbers, Slot, 0);
				}
				PlayMenuCue(Context, EBattleMenuCue::HideComboList, Slot);
				Frame.Pc = ComboClosing;
				Frame.Count = 0;
				break;
			case ComboClosing:
				if (Frame.Count < CloseFrames)
				{
					if (Frame.Count < ComboWindows)
					{
						PlayMenuCue(Context, EBattleMenuCue::CloseListWindow, Slot, Frame.Count);
					}
					++Frame.Count;
					return CallFrames(Context, 1);
				}
				PlayMenuCue(Context, EBattleMenuCue::FreeListWork, Slot);
				PlayMenuCue(Context, EBattleMenuCue::HideArrows, Slot);
				Frame.Pc = ComboClosed;
				break;
			case ComboClosed:
				if (Combo.Result == Left)
				{
					Party.bIconsShown = true;
					Party.ShownState = 0xFF;
					return EFlow::Return;
				}
				BeginStoredCombo(Context, Slot);
				Frame.Pc = ComboStarted;
				break;
			case ComboStarted:
				if (ShowBlocks(Context))
				{
					return EFlow::Yield;
				}
				RunStoredCombo(Context, Slot);
				Frame.Pc = ComboWait;
				break;
			case ComboWait:
				if (!Context.bActionsPlayed)
				{
					return EFlow::Yield;
				}
				Party.bComboClosed = true;
				return EFlow::Return;
			default:
				return EFlow::Return;
			}
		}
	}
}
