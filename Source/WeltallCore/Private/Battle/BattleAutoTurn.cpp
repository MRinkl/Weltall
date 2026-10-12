#include "Battle/BattleAi.h"
#include "Battle/BattlePartySteps.h"
#include "Battle/BattleRules.h"
#include "Battle/BattleSteps.h"
#include "Battle/BattleTurn.h"
#include "Game/GameStateLayout.h"
#include "Game/PsxRandom.h"

namespace Weltall::BattleParty
{
	namespace
	{
		constexpr uint8 NoSlot = 0xFF;
		constexpr uint8 ApproachCode = 0xFD;
		constexpr uint8 FirstAbility = 22;
		constexpr uint16 BoardCommand = 0x0100;
		constexpr uint16 EtherCommand = 0x0020;
		constexpr uint8 Maria = 8;

		enum EAutoKind : uint8
		{
			Nothing = 0,
			AttackKind = 1,
			AbilityKind = 2,
			BoardKind = 4,
		};

		enum EAutoPc : uint8
		{
			AutoStart,
			AutoChoose,
			AutoAct,
			AutoAttackStarted,
			AutoBoarded,
			AutoWait,
		};

		/** The abilities a character's automatic turn may pick: those of the first Count whose bits are in the mask. */
		struct FAutoAbilities
		{
			uint16 Mask = 0;
			uint8 Count = 0;
		};

		bool AutoAbilities(uint8 Character, FAutoAbilities& Abilities)
		{
			switch (Character)
			{
			case 0:
			case 8: Abilities = {0xC000, 2}; return true;
			case 1:
			case 6:
			case 10: Abilities = {0xF000, 4}; return true;
			case 2:
			case 9: Abilities = {0xBFE0, 11}; return true;
			case 3: Abilities = {0xC3C0, 10}; return true;
			case 4: Abilities = {0xDF80, 10}; return true;
			case 5: Abilities = {0x1000, 4}; return true;
			case 7: Abilities = {0xE000, 3}; return true;
			default: return false;
			}
		}

		/** The kind of action a confused member takes and its argument, by the original's rolls. */
		void ChooseAutoAction(FBattleContext& Context, uint8 Slot)
		{
			FBattleAutoTurn& Auto = Context.Party.Auto;
			const FBattleCombatant& Member = Context.Combatants[Slot];
			if (!Member.IsInGear())
			{
				if (BattleRules::Percent(Context) < 10 && (Member.DisabledCommands & BoardCommand) == 0
					&& Context.Party.Disabled[Slot][ECommand::Board] == 0)
				{
					Auto.Kind = BoardKind;
					return;
				}
				FAutoAbilities Abilities;
				if (!AutoAbilities(Member.Character, Abilities))
				{
					Context.Fault = EBattleFault::IndexOutsideTable;
					return;
				}
				if (BattleRules::Percent(Context) < 25 && (Member.DisabledCommands & EtherCommand) == 0)
				{
					Auto.Kind = AbilityKind;
					const int32 Pick = BattleRules::Draw(Context) % Abilities.Count;
					const uint16 Learned = LearnedWord(Context, Member.Character, GameStateLayout::Skills::Abilities);
					if ((Learned & (0x8000u >> Pick) & Abilities.Mask) != 0)
					{
						Auto.Argument = static_cast<uint8>(Pick);
						return;
					}
				}
				if (Member.Character == Maria)
				{
					Auto.Kind = Nothing;
					Auto.Argument = 0;
					return;
				}
			}
			Auto.Kind = AttackKind;
			if (BattleRules::Percent(Context) > 79)
			{
				Auto.Argument = 2;
				return;
			}
			if (BattleRules::Percent(Context) > 59)
			{
				Auto.Argument = 1;
				return;
			}
			Auto.Argument = 0;
		}

		/** A random combatant the member can reach, other than itself (allies too): drawn again until one is. */
		void ChooseRandomTarget(FBattleContext& Context, uint8 Slot)
		{
			std::array<uint8, FBattleContext::SlotCount> Reachable{};
			Reachable.fill(NoSlot);
			uint8 Count = 0;
			bool bOther = false;
			for (uint8 Target = 0; Target < FBattleContext::SlotCount; ++Target)
			{
				if (CanReachTarget(Context, Slot, Target))
				{
					Reachable[Count] = Target;
					++Count;
					bOther = bOther || Target != Slot;
				}
			}
			if (!bOther)
			{
				Context.Fault = EBattleFault::NoOtherTarget;
				return;
			}
			do
			{
				Context.Targeting.Target = Reachable[RandomByteBetween(*Context.Random, 0, 10)];
			} while (Context.Targeting.Target == NoSlot || Context.Targeting.Target == Slot);
		}

		/** An automatic attack begins: the walk queued, the playback started (the show then takes frames). */
		void BeginAutoAttack(FBattleContext& Context, uint8 Slot)
		{
			FBattlePartyInput& Party = Context.Party;
			const uint8 Target = Context.SlotTargets[Slot];
			Party.bApproached = false;
			if (Context.PartyCharacters[Slot] != Billy)
			{
				const uint16 Targets = Bit(Context, Target);
				if (FBattlePerformance* Record = QueueCode(Context, Slot, ApproachCode, 0))
				{
					Record->Targets = Targets;
				}
			}
			Context.CurrentAttack = Context.ApRecords[Slot].InGear == 0 ? 0 : Party.Auto.Argument;
			const bool bStays = BattleSteps::PlanApproachPath(Context, Slot, Target);
			const uint8 Next = FindNextReadySlot(Context, Slot);
			if (Context.Show != nullptr)
			{
				Context.Show->BeginPlayback(bStays, Slot, Target, Next);
			}
		}

		/** The automatic attack's one hit of the chosen strength, then the combo's end. */
		void FinishAutoAttack(FBattleContext& Context, uint8 Slot)
		{
			PerformHit(Context, Slot, static_cast<uint8>(Context.Party.Auto.Argument + 1));
			if (Context.Fault == EBattleFault::None)
			{
				QueueEnd(Context, Slot);
			}
		}

		/** The automatic turn's guard, when it chose nothing it can do. */
		void AutoGuard(FBattleContext& Context, uint8 Slot)
		{
			DefendInBattle(Context, Slot);
			Context.Party.bActed = false;
			Context.Party.bTurnOver = true;
		}
	}

	EFlow RunAutoTurn(FBattleContext& Context, FBattlePartyFrame& Frame)
	{
		FBattlePartyInput& Party = Context.Party;
		const uint8 Slot = Party.Actor;
		for (;;)
		{
			switch (Frame.Pc)
			{
			case AutoStart:
				Frame.Pc = AutoChoose;
				return EFlow::Yield;
			case AutoChoose:
				Party.bActed = true;
				Party.bTurnOver = false;
				EndBattleDefence(Context, Slot);
				ChooseAutoAction(Context, Slot);
				if (Context.Fault != EBattleFault::None)
				{
					return EFlow::Yield;
				}
				if (Party.Auto.Kind == AttackKind || Party.Auto.Kind == AbilityKind)
				{
					ChooseRandomTarget(Context, Slot);
					if (Context.Fault != EBattleFault::None)
					{
						return EFlow::Yield;
					}
				}
				ResetQueue(Context, Slot, Context.Targeting.Target);
				Context.SlotTargets[Slot] = Context.Targeting.Target;
				FrameCamera(Context, static_cast<uint16>(Bit(Context, Slot) | Bit(Context, Context.SlotTargets[Slot])));
				Frame.Pc = AutoAct;
				return EFlow::Yield;
			case AutoAct:
				Party.bAutoTurn = true;
				Frame.Pc = AutoWait;
				if (Party.Auto.Kind == AbilityKind)
				{
					FBattleCombatant& Member = Context.Combatants[Slot];
					const uint8 Cost = BattleRules::CommandAt(Context, FBattleContext::PartyCommandIndex(Slot, Party.Auto.Argument + FirstAbility)).EpCost;
					if (Cost <= Member.Ep)
					{
						Member.Ep = static_cast<uint16>(Member.Ep - Cost);
						Party.Choice = Party.Auto.Argument;
						return Call(Context, EBattlePartyRoutine::PerformAbility);
					}
					AutoGuard(Context, Slot);
				}
				else if (Party.Auto.Kind == AttackKind)
				{
					BeginAutoAttack(Context, Slot);
					Frame.Pc = AutoAttackStarted;
				}
				else if (Party.Auto.Kind == BoardKind)
				{
					BeginBoarding(Context, Slot);
					Frame.Pc = AutoBoarded;
				}
				else
				{
					AutoGuard(Context, Slot);
				}
				break;
			case AutoAttackStarted:
				if (ShowBlocks(Context))
				{
					return EFlow::Yield;
				}
				FinishAutoAttack(Context, Slot);
				Frame.Pc = AutoWait;
				break;
			case AutoBoarded:
				if (Context.Fault != EBattleFault::None || ShowBlocks(Context))
				{
					return EFlow::Yield;
				}
				FinishBoarding(Context, Slot);
				Party.bTurnOver = true;
				Frame.Pc = AutoWait;
				break;
			case AutoWait:
				if (!Party.bTurnOver && Context.End == BattleEnd::Running && !Context.bActionsPlayed)
				{
					return EFlow::Yield;
				}
				Party.bAutoTurn = false;
				return EFlow::Return;
			default:
				return EFlow::Return;
			}
		}
	}
}
