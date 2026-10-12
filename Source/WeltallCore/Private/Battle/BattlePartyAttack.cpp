#include "Battle/BattleAction.h"
#include "Battle/BattleAi.h"
#include "Battle/BattleParty.h"
#include "Battle/BattlePartySteps.h"
#include "Battle/BattleProgram.h"
#include "Battle/BattleSteps.h"
#include "Game/GameStateLayout.h"

namespace Weltall::BattleParty
{
	namespace
	{
		constexpr uint8 ReactionCode = 0xF3;
		constexpr uint8 ApproachCode = 0xFD;
		constexpr uint16 ShowsReactions = 0x0800;
		/** The AP bar's full height in its table's units, as a percent. */
		constexpr uint32 ApBarFull = 0x38;
		/** The Fierce basic attack's command + 1, which a Deathblow no longer learned falls back to. */
		constexpr uint8 FierceAttack = 7;
		constexpr uint8 FirstDeathblowAttack = 8;
		constexpr uint8 NoPattern = FBattleProgramTables::PatternCount;
		constexpr uint8 AmmunitionWindow = 5;

		enum EAttackPc : uint8
		{
			AttackStart,
			AttackShown,
			AttackAfterCancel,
			AttackAfterButton,
		};

		/** Bit n of a learned-skills word, as the game's table of the high bits gives it (none from 16). */
		bool HighBit(uint16 Word, uint8 Bit)
		{
			return Bit < 16 && (Word & (0x8000u >> Bit)) != 0;
		}

		/** The pattern the buttons pressed so far make (NoPattern for none). */
		uint8 MatchPattern(const FBattleContext& Context)
		{
			const auto& Patterns = Context.Program->Patterns;
			for (uint8 Pattern = 0; Pattern < Patterns.size(); ++Pattern)
			{
				if (Patterns[Pattern] == Context.Party.Buttons)
				{
					return Pattern;
				}
			}
			return NoPattern;
		}

		/** The Deathblow pattern Pattern completes for the slot's character; a character past the lists faults. */
		uint8 PatternDeathblow(FBattleContext& Context, uint8 Slot, uint8 Pattern)
		{
			const uint8 Character = Context.Combatants[Slot].Character;
			if (Character >= FBattleProgramTables::DeathblowListCount)
			{
				Context.Fault = EBattleFault::IndexOutsideTable;
				return 0;
			}
			return Context.Program->Deathblows[Character][Pattern];
		}

		uint16 LearnedDeathblows(FBattleContext& Context, uint8 Slot)
		{
			return LearnedWord(Context, Context.PartyCharacters[Slot], GameStateLayout::Skills::Deathblows);
		}

		/** A Fierce press finishes a learned Deathblow's pattern. */
		bool IsDeathblowFinisher(FBattleContext& Context, uint8 Event, uint8 Slot)
		{
			if (Context.Party.ButtonCount == 0 || Event != BattleMenuEvents::Confirm)
			{
				return false;
			}
			const uint8 Pattern = MatchPattern(Context);
			return Pattern != NoPattern && HighBit(LearnedDeathblows(Context, Slot), PatternDeathblow(Context, Slot, Pattern));
		}

		/** The press joins the turn's buttons; when it makes a Deathblow, the attack becomes that Deathblow. */
		void RecordButton(FBattleContext& Context, uint8 Event, uint8 Slot)
		{
			FBattlePartyInput& Party = Context.Party;
			if (Party.ButtonCount >= Party.Buttons.size())
			{
				Context.Fault = EBattleFault::IndexOutsideTable;
				return;
			}
			const uint8 Button = static_cast<uint8>(Event - BattleMenuEvents::Confirm);
			Party.Buttons[Party.ButtonCount] = Button;
			++Party.ButtonCount;
			if (Party.Deathblow == 0)
			{
				return;
			}
			Party.Buttons[Party.ButtonCount - 1] = 0xFF;
			const uint8 Pattern = MatchPattern(Context);
			Party.Buttons[Party.ButtonCount - 1] = Button;
			if (Pattern == NoPattern)
			{
				return;
			}
			const uint8 Deathblow = PatternDeathblow(Context, Slot, Pattern);
			if (HighBit(LearnedDeathblows(Context, Slot), Deathblow) && !IsDisabled(Context, Slot, ECommand::FierceButton))
			{
				Context.CurrentAttack = static_cast<uint8>(Deathblow + FirstDeathblowAttack);
			}
		}

		/** A press's AP by its event (0 for none). */
		uint8 PressCost(uint8 Event)
		{
			switch (Event)
			{
			case BattleMenuEvents::Confirm: return 3;
			case BattleMenuEvents::Strong: return 2;
			case BattleMenuEvents::Weak: return 1;
			default: return 0;
			}
		}

		int32 ButtonCommand(uint8 Event)
		{
			switch (Event)
			{
			case BattleMenuEvents::Confirm: return ECommand::FierceButton;
			case BattleMenuEvents::Weak: return ECommand::WeakButton;
			default: return ECommand::StrongButton;
			}
		}

		/** The AP bar's height (in percent) once the press is paid; a turn's AP past the table faults. */
		uint16 ApBarHeight(FBattleContext& Context)
		{
			const FBattlePartyInput& Party = Context.Party;
			if (Party.ApAtStart >= FBattleProgramTables::ApBarRows || Party.ApLeft >= FBattleProgramTables::ApBarColumns)
			{
				Context.Fault = EBattleFault::IndexOutsideTable;
				return 0;
			}
			return static_cast<uint16>(Context.Program->ApBar[Party.ApAtStart][Party.ApLeft] * 100u / ApBarFull);
		}

		/** The press accepted: the target locked, the AP paid, the button recorded (whose hints take a frame). */
		EFlow TakePress(FBattleContext& Context, FBattlePartyFrame& Frame, uint8 Slot, uint8 Event)
		{
			FBattlePartyInput& Party = Context.Party;
			PlaySound(Context, ESound::Accept);
			LockTarget(Context, Slot);
			if (!Party.bIconsShown)
			{
				Party.MenuState = EState::AttackMode;
				Party.ShownState = EState::AttackMode;
			}
			else
			{
				Party.MenuState = EState::FadingAttackMode;
				Party.ShownState = 0xFF;
				Party.bMenuFading = true;
			}
			Party.ApLeft = static_cast<uint8>(Party.ApLeft - Party.Cost);
			PlayMenuCue(Context, EBattleMenuCue::ApNumbers, Slot);
			RecordButton(Context, Event, Slot);
			PlayMenuCue(Context, EBattleMenuCue::DeathblowHints, Slot);
			Frame.Pc = AttackAfterButton;
			return EFlow::Yield;
		}

		/** The combo's end queued when the AP is spent or the hit ends it. */
		EFlow CloseIfDone(FBattleContext& Context, uint8 Slot)
		{
			FBattlePartyInput& Party = Context.Party;
			if (Party.ApLeft == 0)
			{
				QueueEnd(Context, Slot);
				Party.bComboClosed = true;
			}
			if (Party.bHitEndsCombo)
			{
				QueueEnd(Context, Slot);
				Party.bComboClosed = true;
			}
			return EFlow::Return;
		}
	}

	void BeginAttack(FBattleContext& Context, uint8 Slot)
	{
		FBattlePartyInput& Party = Context.Party;
		PlayMenuCue(Context, EBattleMenuCue::ApNumbers, Slot);
		const uint8 Target = Context.SlotTargets[Slot];
		ResetQueue(Context, Slot, Target);
		Party.bMenuSounds = false;
		const bool bStays = BattleSteps::PlanApproachPath(Context, Slot, Target);
		const uint8 Next = FindNextReadySlot(Context, Slot);
		if (Context.Show != nullptr)
		{
			Context.Show->BeginPlayback(bStays, Slot, Target, Next);
		}
	}

	void FinishAttackStart(FBattleContext& Context)
	{
		Context.Party.bMenuSounds = true;
		Context.Party.bApproached = false;
	}

	EFlow RunStartAttack(FBattleContext& Context, FBattlePartyFrame& Frame)
	{
		FBattlePartyInput& Party = Context.Party;
		const uint8 Slot = Party.Actor;
		const bool bInGear = Party.MenuState == EState::GearAttack;
		if (Frame.Pc == 0)
		{
			if (!bInGear)
			{
				Party.bMenuSounds = false;
			}
			BeginAttack(Context, Slot);
			Frame.Pc = 1;
		}
		if (ShowBlocks(Context))
		{
			return EFlow::Yield;
		}
		FinishAttackStart(Context);
		InitTargetCursor(Context, Slot);
		PlayMenuCue(Context, EBattleMenuCue::InitArrows, Slot);
		Party.MenuState = bInGear ? EState::GearAttackMode : EState::AttackMode;
		if (bInGear)
		{
			Party.bMenuSounds = false;
		}
		return EFlow::Return;
	}

	void InitTargetCursor(FBattleContext& Context, uint8 Slot)
	{
		FBattlePartyInput& Party = Context.Party;
		Party.Cursor = Context.SlotTargets[Slot];
		PickBattleDefaultTarget(Context, Slot);
		const FBattleTargeting& Targeting = Context.Targeting;
		for (uint8 Index = 0; Index < Targeting.Count && Index < Targeting.List.size(); ++Index)
		{
			if (Targeting.List[Index] == Context.SlotTargets[Slot])
			{
				return;
			}
		}
		Party.Cursor = Targeting.List[0];
	}

	void LockTarget(FBattleContext& Context, uint8 Slot)
	{
		FBattlePartyInput& Party = Context.Party;
		if (Party.bTargetLocked)
		{
			return;
		}
		Party.bTargetLocked = true;
		SetMarkers(Context, 0);
		Context.SlotTargets[Slot] = Party.Cursor;
		PlayMenuCue(Context, EBattleMenuCue::HideArrows, Slot);
		FrameCamera(Context, static_cast<uint16>(Bit(Context, Slot) | Bit(Context, Context.SlotTargets[Slot])));
		if (Context.PartyCharacters[Slot] != Billy)
		{
			const uint16 Targets = Bit(Context, Context.SlotTargets[Slot]);
			if (FBattlePerformance* Record = QueueCode(Context, Slot, ApproachCode, 0))
			{
				Record->Targets = Targets;
			}
		}
		CloseAmmunition(Context, Slot, false);
	}

	bool PerformHit(FBattleContext& Context, uint8 Slot, uint8 Cost)
	{
		FBattlePartyInput& Party = Context.Party;
		const uint8 Target = Context.SlotTargets[Slot];
		const bool bOnFoot = Context.ApRecords[Slot].InGear == 0;
		bool bEnds = false;
		if (Target >= FBattleContext::SlotCount)
		{
			Context.Fault = EBattleFault::SlotOutsideBattle;
			return false;
		}
		if (!Party.bApproached)
		{
			if (!bOnFoot)
			{
				BattleSteps::MoveGearToTargetGroup(Context, Slot, Target);
			}
			else if (Context.PartyCharacters[Slot] != Billy)
			{
				BattleSteps::MoveToTargetGroup(Context, Slot, Target);
			}
			Party.bApproached = true;
		}
		if (CurrentRecord(Context) == nullptr)
		{
			return false;
		}
		ClearBattlePerformance(Context);
		if (bOnFoot)
		{
			if (Context.CurrentAttack < FirstDeathblowAttack)
			{
				Context.CurrentAttack = Context.Program->BasicAttacks[Context.CurrentAttack * 3u + Cost - 1u];
			}
			else if (HighBit(LearnedDeathblows(Context, Slot), static_cast<uint8>(Context.CurrentAttack - FirstDeathblowAttack)))
			{
				bEnds = true;
			}
			else
			{
				Context.CurrentAttack = FierceAttack;
			}
		}
		else
		{
			Context.CurrentAttack = static_cast<uint8>(Context.CurrentAttack + 1);
		}
		if ((Context.Combatants[Target].Traits & ShowsReactions) != 0)
		{
			QueueCode(Context, Slot, ReactionCode, Bit(Context, Target));
		}
		const uint8 Command = static_cast<uint8>(Context.CurrentAttack - 1);
		PerformBattleCommand(Context, Slot, Command, Bit(Context, Target), Command);
		FBattlePerformance* Record = CurrentRecord(Context);
		if (Context.Fault != EBattleFault::None || Record == nullptr)
		{
			return false;
		}
		const uint8 Committed = Context.PerformanceIndex;
		Record->Actor = Slot;
		Record->Targets = Context.Action.Targets;
		Record->Code = static_cast<uint8>(Context.Action.Animation);
		++Context.PerformanceIndex;
		RecordBattleHit(Context, Slot, Target);
		if (Target >= FBattleContext::PartySlotCount && RunBattleHitScript(Context, Target))
		{
			bEnds = true;
		}
		if (Context.Fault == EBattleFault::None)
		{
			CommitBattleResults(Context, Committed);
		}
		return bEnds;
	}

	EFlow ShowTarget(FBattleContext& Context, uint8 Slot)
	{
		FBattlePartyInput& Party = Context.Party;
		if (Party.bTargetLocked)
		{
			return EFlow::Return;
		}
		FrameCamera(Context, Bit(Context, Party.Cursor));
		SetMarkers(Context, Bit(Context, Party.Cursor));
		uint8 Ways = 0;
		for (uint8 Way = 0; Way < 4; ++Way)
		{
			if (FindTargetInDirection(Context, Party.Cursor, Way) != Party.Cursor)
			{
				Ways = static_cast<uint8>(Ways | 1u << Way);
			}
		}
		PlayMenuCue(Context, EBattleMenuCue::ArrowTargets, Slot, Ways);
		if (Context.GearTurn.Level != InfinityLevel && Context.PartyCharacters[Slot] == Billy && Party.ListWindow == 0)
		{
			return Call(Context, EBattlePartyRoutine::OpenAmmunition);
		}
		return EFlow::Return;
	}

	EFlow CloseAmmunition(FBattleContext& Context, uint8 Slot, bool bClose)
	{
		FBattlePartyInput& Party = Context.Party;
		if (Context.PartyCharacters[Slot] != Billy)
		{
			return EFlow::Return;
		}
		Party.ListWindow = 0;
		PlayMenuCue(Context, EBattleMenuCue::HideAmmunition, Slot);
		if (bClose && Party.bAmmunitionShown)
		{
			return Call(Context, EBattlePartyRoutine::CloseAmmunition);
		}
		return EFlow::Return;
	}

	EFlow RunOpenAmmunition(FBattleContext& Context, FBattlePartyFrame& Frame)
	{
		if (Frame.Pc == 0)
		{
			PlayMenuCue(Context, EBattleMenuCue::BuildAmmunition, Context.Party.Actor);
			Frame.Pc = 1;
			return EFlow::Yield;
		}
		Context.Party.ListWindow = AmmunitionWindow;
		Context.Party.bAmmunitionShown = true;
		return EFlow::Return;
	}

	EFlow RunCloseAmmunition(FBattleContext& Context, FBattlePartyFrame& Frame)
	{
		if (Frame.Pc < 2)
		{
			if (Frame.Pc == 0)
			{
				PlayMenuCue(Context, EBattleMenuCue::CloseListWindow, Context.Party.Actor, 0);
			}
			++Frame.Pc;
			return EFlow::Yield;
		}
		CloseBattleAmmunitionWindow(Context);
		return EFlow::Return;
	}

	EFlow RunAttackMode(FBattleContext& Context, FBattlePartyFrame& Frame)
	{
		FBattlePartyInput& Party = Context.Party;
		const uint8 Slot = Party.Actor;
		switch (Frame.Pc)
		{
		case AttackStart:
			Party.bMenuSounds = false;
			if (Party.bComboClosed)
			{
				return EFlow::Return;
			}
			Party.Cost = 0;
			Frame.Pc = AttackShown;
			if (ShowTarget(Context, Slot) == EFlow::Call)
			{
				return EFlow::Call;
			}
			[[fallthrough]];
		case AttackShown:
		{
			const uint8 Event = Party.Event;
			Frame.Pc = AttackAfterCancel;
			if (Event == BattleMenuEvents::Cancel)
			{
				if (Party.ApLeft == Party.ApAtStart)
				{
					if (Context.Show != nullptr)
					{
						Context.Show->CancelPlayback();
					}
					Party.bMenuSounds = true;
					PlayMenuCue(Context, EBattleMenuCue::CancelApNumbers, Slot);
					Party.MenuState = EState::Attack;
					PlayMenuCue(Context, EBattleMenuCue::HideArrows, Slot);
					if (CloseAmmunition(Context, Slot, true) == EFlow::Call)
					{
						return EFlow::Call;
					}
				}
				else
				{
					QueueEnd(Context, Slot);
					Party.bComboClosed = true;
				}
			}
			else if (PressCost(Event) != 0 && IsDisabled(Context, Slot, ButtonCommand(Event)))
			{
				Party.Event = BattleMenuEvents::Cancel;
				PlaySound(Context, ESound::Refuse);
			}
			[[fallthrough]];
		}
		case AttackAfterCancel:
		{
			const uint8 Event = Party.Event;
			if (!Party.bTargetLocked)
			{
				if (Event <= BattleMenuEvents::Up)
				{
					Party.Cursor = FindTargetInDirection(Context, Party.Cursor, Event);
					ResetQueue(Context, Context.TurnActor, Party.Cursor);
					PlaySound(Context, ESound::Move);
				}
				if (Party.Cursor >= FBattleContext::SlotCount)
				{
					Context.Fault = EBattleFault::SlotOutsideBattle;
					return EFlow::Yield;
				}
				BattleSteps::PlanApproachPath(Context, Slot, Party.Cursor);
			}
			PlayMenuCue(Context, EBattleMenuCue::ShowApNumbers, Slot, 1);
			if (Event == BattleMenuEvents::Cancel || PressCost(Event) == 0)
			{
				return EFlow::Return;
			}
			if (Event == BattleMenuEvents::Confirm && IsDeathblowFinisher(Context, Event, Slot) && Party.ApLeft >= 3)
			{
				Party.Deathblow = static_cast<uint8>(Party.Deathblow + 1);
			}
			Party.Cost = PressCost(Event);
			if (Party.ApLeft < Party.Cost)
			{
				PlaySound(Context, ESound::Refuse);
				return CloseIfDone(Context, Slot);
			}
			return TakePress(Context, Frame, Slot, Event);
		}
		case AttackAfterButton:
			Party.bHitEndsCombo = PerformHit(Context, Slot, Party.Cost);
			if (Context.Fault != EBattleFault::None)
			{
				return EFlow::Yield;
			}
			Context.Waits[Slot] = ApBarHeight(Context);
			Party.bStoresAp = true;
			return CloseIfDone(Context, Slot);
		default:
			return EFlow::Return;
		}
	}
}
