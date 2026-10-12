#include "Battle/BattleParty.h"
#include "Battle/BattlePartySteps.h"
#include "Battle/BattleProgram.h"
#include "Game/GameStateLayout.h"

namespace Weltall::BattleParty
{
	namespace
	{
		constexpr uint8 NoButton = 0xFF;
		/** The Infinity attacks' place in the gears' attack commands, one per button. */
		constexpr uint32 InfinityAttacks = 12;
		/** The fuel costs of the follow-ups start after the three buttons' own. */
		constexpr uint32 FollowUpCosts = 3;

		enum EGearPc : uint8
		{
			GearStart,
			GearShown,
			GearAfterCancel,
			GearAfterButton,
			GearAfterOpen,
		};

		bool HighBit(uint16 Word, uint8 Bit)
		{
			return Bit < 16 && (Word & (0x8000u >> Bit)) != 0;
		}

		/** A gear attack command by its place (a place past the table faults). */
		uint8 GearAttack(FBattleContext& Context, uint32 Index)
		{
			const uint32 Place = Index & 0xFF;
			if (Place >= FBattleProgramTables::GearAttackCount)
			{
				Context.Fault = EBattleFault::IndexOutsideTable;
				return 0;
			}
			return Context.Program->GearAttacks[Place];
		}

		uint16 GearCost(FBattleContext& Context, uint32 Index)
		{
			if (Index >= Context.GearTurn.Costs.size())
			{
				Context.Fault = EBattleFault::IndexOutsideTable;
				return 0;
			}
			return Context.GearTurn.Costs[Index];
		}

		uint16 LearnedGearAttacks(FBattleContext& Context, uint8 Slot)
		{
			return LearnedWord(Context, Context.PartyCharacters[Slot], GameStateLayout::Skills::GearDeathblows);
		}

		/** Whether a press may follow the first: a learned follow-up the attack level allows (any learned Infinity attack at level 4). */
		bool CanFollowUp(FBattleContext& Context, uint8 Button, uint8 Slot)
		{
			const FBattlePartyInput& Party = Context.Party;
			const uint8 Level = Context.GearTurn.Level;
			uint32 Index = Button + (Party.Buttons[0] + 1u) * 3u;
			if (Party.ButtonCount == 0)
			{
				return true;
			}
			if (Level == InfinityLevel)
			{
				Index = Button + InfinityAttacks;
			}
			if (!HighBit(LearnedGearAttacks(Context, Slot), GearAttack(Context, Index)))
			{
				return false;
			}
			return Party.Buttons[0] == NoButton || Level == InfinityLevel || Party.Buttons[0] + 1u <= Level;
		}

		/**
		 * The press's attack: the button's own, or the learned follow-up the second press makes (then 0: the
		 * combo ends). Otherwise returns whether any follow-up is left; bFrame tells whether the hints took a frame.
		 */
		uint8 RecordGearButton(FBattleContext& Context, uint8 Button, uint8 Slot, bool& bFrame)
		{
			FBattlePartyInput& Party = Context.Party;
			const uint8 Level = Context.GearTurn.Level;
			const uint8 Target = Context.SlotTargets[Slot];
			bFrame = false;
			if (Target >= FBattleContext::SlotCount)
			{
				Context.Fault = EBattleFault::SlotOutsideBattle;
				return 0;
			}
			if (!Context.Combatants[Target].IsInGear() && Level != InfinityLevel)
			{
				Context.CurrentAttack = GearAttack(Context, Button);
				return 1;
			}
			if ((Level == 0 || static_cast<int32>(Level) - 1 < static_cast<int32>(Button)) && Party.GearHits == 0)
			{
				Context.CurrentAttack = Button;
				return 1;
			}
			if (Party.ButtonCount >= Party.Buttons.size())
			{
				Context.Fault = EBattleFault::IndexOutsideTable;
				return 0;
			}
			Party.Buttons[Party.ButtonCount] = Button;
			++Party.ButtonCount;
			uint32 Index = (Button + 1u) * 3u;
			if (Party.Deathblow != 0)
			{
				Index = Button + (Party.Buttons[0] + 1u) * 3u;
			}
			if (Level == InfinityLevel)
			{
				Index = Button == NoButton ? InfinityAttacks : Button + InfinityAttacks;
			}
			const uint16 Learned = LearnedGearAttacks(Context, Slot);
			bool bFollowUp = HighBit(Learned, GearAttack(Context, Index));
			bFrame = true;
			PlayMenuCue(Context, EBattleMenuCue::GearHints, Slot, Button);
			if (Party.Deathblow == 0 || !bFollowUp)
			{
				Context.CurrentAttack = Button;
				const uint32 Strong = Level == InfinityLevel ? InfinityAttacks + 1 : (Button + 1u) * 3u + 1;
				if (HighBit(Learned, GearAttack(Context, Strong)) && !IsDisabled(Context, Slot, ECommand::StrongButton))
				{
					bFollowUp = true;
				}
				const uint32 Fierce = Level == InfinityLevel ? InfinityAttacks + 2 : (Button + 1u) * 3u + 2;
				if (HighBit(Learned, GearAttack(Context, Fierce)) && !IsDisabled(Context, Slot, ECommand::FierceButton))
				{
					bFollowUp = true;
				}
				return bFollowUp ? 1 : 0;
			}
			if (Party.Buttons[0] == NoButton)
			{
				Index = Button + InfinityAttacks;
			}
			Context.CurrentAttack = GearAttack(Context, Index);
			return 0;
		}

		uint8 PressButton(uint8 Event)
		{
			switch (Event)
			{
			case BattleMenuEvents::Weak: return 0;
			case BattleMenuEvents::Strong: return 1;
			case BattleMenuEvents::Confirm: return 2;
			default: return NoButton;
			}
		}

		void CloseCombo(FBattleContext& Context, uint8 Slot)
		{
			QueueEnd(Context, Slot);
			Context.Party.bComboClosed = true;
		}

		/** How a press went: refused, taken with its hints' frame to come, or taken with the hit at once. */
		enum class EPress : uint8
		{
			Refused,
			Framed,
			Immediate,
		};

		/** The hit of a press taken, once its hints are shown: the fuel paid; a follow-up Deathblow closes the combo. */
		EFlow Hit(FBattleContext& Context, FBattlePartyFrame& Frame, uint8 Slot)
		{
			FBattlePartyInput& Party = Context.Party;
			Party.Deathblow = Frame.Count == 0 ? 1 : 0;
			PerformHit(Context, Slot, 0);
			if (Context.Fault != EBattleFault::None)
			{
				return EFlow::Yield;
			}
			FBattleGearPart& Gear = Context.Combatants[Slot].GearPart;
			Gear.Fuel = static_cast<uint16>(Gear.Fuel - GearCost(Context, static_cast<uint8>(Context.CurrentAttack - 1)));
			PlayMenuCue(Context, EBattleMenuCue::CockpitFuel, Slot);
			Party.GearHits = static_cast<uint8>(Party.GearHits + 1);
			if (Party.Deathblow != 0)
			{
				CloseCombo(Context, Slot);
			}
			return EFlow::Return;
		}

		/** A gear press: the follow-up checked, the fuel checked, the target locked and the button recorded. */
		EPress Press(FBattleContext& Context, FBattlePartyFrame& Frame, uint8 Slot, uint8 Button)
		{
			FBattlePartyInput& Party = Context.Party;
			const uint8 Level = Context.GearTurn.Level;
			if (!CanFollowUp(Context, Button, Slot))
			{
				return EPress::Refused;
			}
			uint32 CostIndex = Button;
			if (Party.ButtonCount != 0)
			{
				CostIndex = Level == InfinityLevel ? Button + InfinityAttacks : Party.Buttons[0] * 3u + Button + FollowUpCosts;
			}
			const uint16 Cost = GearCost(Context, CostIndex);
			if (Context.Combatants[Slot].GearPart.Fuel < Cost)
			{
				Party.bMenuSounds = true;
				PlayMenuSound(Context, ESound::Refuse);
				Party.bMenuSounds = false;
			}
			else
			{
				PlaySound(Context, ESound::Accept);
				Party.bGearPressed = true;
				LockTarget(Context, Slot);
				if (Party.ButtonCount != 0)
				{
					Party.Deathblow = static_cast<uint8>(Party.Deathblow + 1);
				}
				if (!Party.bIconsShown)
				{
					Party.MenuState = EState::GearAttackMode;
					Party.ShownState = EState::GearAttackMode;
				}
				else
				{
					Party.MenuState = EState::FadingGearAttackMode;
					Party.ShownState = 0xFF;
					Party.bMenuFading = true;
				}
				bool bFrame = false;
				Frame.Count = RecordGearButton(Context, Button, Slot, bFrame);
				Frame.Pc = GearAfterButton;
				return bFrame ? EPress::Framed : EPress::Immediate;
			}
			if (Party.Deathblow != 0)
			{
				CloseCombo(Context, Slot);
			}
			return EPress::Refused;
		}
	}

	EFlow RunGearAttackMode(FBattleContext& Context, FBattlePartyFrame& Frame)
	{
		FBattlePartyInput& Party = Context.Party;
		const uint8 Slot = Party.Actor;
		const uint8 Level = Context.GearTurn.Level;
		switch (Frame.Pc)
		{
		case GearStart:
			Party.bMenuSounds = false;
			if (Party.bComboClosed)
			{
				return EFlow::Return;
			}
			Frame.Pc = GearShown;
			if (ShowTarget(Context, Slot) == EFlow::Call)
			{
				return EFlow::Call;
			}
			[[fallthrough]];
		case GearShown:
		{
			const uint8 Event = Party.Event;
			Frame.Pc = GearAfterCancel;
			if (Event == BattleMenuEvents::Cancel)
			{
				if (Party.GearHits != 0 && Level != InfinityLevel)
				{
					CloseCombo(Context, Slot);
				}
				else
				{
					PlayMenuCue(Context, EBattleMenuCue::HideHints, Slot);
					if (Context.Show != nullptr)
					{
						Context.Show->CancelPlayback();
					}
					Party.bMenuSounds = true;
					Party.MenuState = EState::GearAttack;
					PlayMenuCue(Context, EBattleMenuCue::HideArrows, Slot);
					if (Level == InfinityLevel)
					{
						Party.GearHits = 0;
						Party.ButtonCount = 0;
					}
					else if (CloseAmmunition(Context, Slot, true) == EFlow::Call)
					{
						return EFlow::Call;
					}
				}
			}
			else if (PressButton(Event) != NoButton)
			{
				static constexpr std::array<int32, 3> Commands{ECommand::WeakButton, ECommand::StrongButton, ECommand::FierceButton};
				if (IsDisabled(Context, Slot, Commands[PressButton(Event)]))
				{
					Party.Event = BattleMenuEvents::Cancel;
					PlaySound(Context, ESound::Refuse);
				}
			}
			[[fallthrough]];
		}
		case GearAfterCancel:
		{
			const uint8 Event = Party.Event;
			if (!Party.bTargetLocked && Event <= BattleMenuEvents::Up)
			{
				Party.Cursor = FindTargetInDirection(Context, Party.Cursor, Event);
				PlaySound(Context, ESound::Move);
			}
			Context.PanelRefresh[Slot] = 1;
			if (Party.GearHits != 0)
			{
				if (Level != InfinityLevel && (Level == 0 || static_cast<int32>(Level) - 1 < static_cast<int32>(Party.Buttons[0])))
				{
					CloseCombo(Context, Slot);
					return EFlow::Return;
				}
				const uint8 Target = Context.SlotTargets[Slot];
				if (Target < 16 && (Context.Action.KnockedOut & (1u << Target)) != 0)
				{
					CloseCombo(Context, Slot);
					return EFlow::Return;
				}
			}
			if (Event == BattleMenuEvents::Cancel)
			{
				return EFlow::Return;
			}
			const uint8 Button = PressButton(Event);
			if (Level != InfinityLevel || Party.GearHits != 0)
			{
				if (Button == NoButton)
				{
					return EFlow::Return;
				}
				switch (Press(Context, Frame, Slot, Button))
				{
				case EPress::Framed: return EFlow::Yield;
				case EPress::Immediate: return Hit(Context, Frame, Slot);
				case EPress::Refused: return EFlow::Return;
				}
				return EFlow::Return;
			}
			Party.GearHits = static_cast<uint8>(Party.GearHits + 1);
			bool bFrame = false;
			RecordGearButton(Context, NoButton, Slot, bFrame);
			Frame.Pc = GearAfterOpen;
			return bFrame ? EFlow::Yield : EFlow::Return;
		}
		case GearAfterButton:
			return Hit(Context, Frame, Slot);
		case GearAfterOpen:
		default:
			return EFlow::Return;
		}
	}
}
