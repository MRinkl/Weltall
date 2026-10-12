#include "Battle/BattleItems.h"

#include "Battle/BattleGameState.h"
#include "Battle/BattleRules.h"
#include "Battle/BattleSteps.h"

namespace Weltall
{
	using namespace BattleRules;

	namespace
	{
		constexpr uint8 Citan = 2;
		constexpr uint16 PlayDead = 0x0002;
		/** The show's number for an item that has no effect on a gear. */
		constexpr uint16 NoEffectInGearAnimation = 0xC2;
		constexpr uint16 NoEffectInGear = 0x8000;

		/** The status words an item's clear can reach: 84h to 8Eh. */
		constexpr uint8 LastClearableWord = 5;

		/** A status's turns by the action's actor and target, or with the fix by the item's user and the slot it lands on. */
		void SetItemStatusDuration(FBattleContext& Context, uint8 User, uint8 Slot, uint8 Group, uint16 Bits, uint8 Turns)
		{
			if (Context.RuleOptions.bItemBuffsByItsUser)
			{
				BattleSteps::SetStatusDurationBy(Context, Context.Combatants[User], &Context.Combatants[Slot], Slot, Group, Bits, Turns);
			}
			else
			{
				BattleSteps::SetStatusDuration(Context, Slot, Group, Bits, Turns);
			}
		}

		void ApplySpecial(FBattleContext& Context, uint8 User, uint8 Slot, const FBattleItem& Item)
		{
			FBattleCombatant& Target = Context.Combatants[Slot];
			switch (Item.Power)
			{
			case 10:
				Target.Weakness = Item.Parameter;
				break;
			case 11:
				if ((Target.StatusImmunity & Item.Parameter) == 0)
				{
					Target.Status |= Item.Parameter;
					SetItemStatusDuration(Context, User, Slot, 0, Item.Parameter, Item.Setting);
					BattleSteps::SetStatusMessage(Context, 0, Item.Parameter);
				}
				break;
			case 12:
				if ((Target.MentalImmunity & Item.Parameter) == 0)
				{
					Target.Mental |= Item.Parameter;
					SetItemStatusDuration(Context, User, Slot, 2, Item.Parameter, Item.Setting);
					BattleSteps::SetStatusMessage(Context, 2, Item.Parameter);
				}
				break;
			case 13:
				Target.GearPart.ArmorBreak = static_cast<uint8>(Target.GearPart.ArmorBreak + Item.Parameter);
				Context.Action.Message = 0x28;
				break;
			case 14:
			{
				using namespace GameStateLayout;
				if (!BattleGameState::HasCharacter(Target.Character))
				{
					Context.Fault = EBattleFault::OutsideGameState;
					return;
				}
				const std::size_t Record = CharacterOffset(Target.Character);
				LittleEndian::WriteU32(Context.GameState->Block(), Record + Character::ExpToNext, 1);
				LittleEndian::WriteU32(Context.GameState->Block(), Record + Character::EtherExpToNext, 1);
				break;
			}
			case 15:
				for (uint16& Counter : Target.DeathblowCounters)
				{
					Counter = static_cast<uint16>(Counter + 10);
				}
				break;
			default:
				break;
			}
		}

		void ApplyItem(FBattleContext& Context, uint8 User, uint8 Slot, const FBattleItem& Item)
		{
			FBattleAction& Action = Context.Action;
			FBattleCombatant& Target = Context.Combatants[Slot];
			const uint16 Effect = Item.Effect;
			int32 Heals = 0;
			if ((Effect & 0x8000) != 0)
			{
				Action.Results[Slot] = BattleResults::HpHeal;
				Action.Amounts[Slot] = static_cast<uint32>(Item.Power) * 50;
				if ((Target.PermanentBuffs & 0x80) != 0)
				{
					Action.Amounts[Slot] <<= 1;
				}
				Heals = 1;
				if ((Target.Absorption & 0x8000) != 0)
				{
					Action.Results[Slot] = BattleResults::HpDamage;
				}
			}
			if ((Effect & 0x4000) != 0)
			{
				++Heals;
				Action.Results[Slot] = BattleResults::EpHeal;
				Action.Amounts[Slot] = static_cast<uint32>(Item.Power) * 10;
			}
			if (Heals != 0)
			{
				return;
			}
			if (Target.IsInGear() && Effect != 1)
			{
				Action.Revived |= NoEffectInGear;
				Action.Message = 0x30;
				return;
			}
			if ((Effect & 0x2000) != 0)
			{
				Target.Status &= 0x8000;
			}
			if ((Effect & 0x1000) != 0)
			{
				Target.Mental = 0;
				Target.DisabledCommands &= 0xFFDF;
			}
			if ((Effect & 0x100) != 0)
			{
				if (Target.Character == Citan)
				{
					BattleSteps::ResetEtherTargetAll(Context, Slot, false);
				}
				Target.Status = 0;
				Target.Mental = 0;
				Target.Buffs = 0;
				Target.EtherBuffs = 0;
				Target.Elements = 0;
				Target.Hp = static_cast<uint16>(static_cast<uint32>(Target.MaxHp) * Item.Power / 10);
				Action.Revived = static_cast<uint16>(Action.Revived | SlotBit(Slot));
			}
			if ((Effect & 0x800) != 0)
			{
				Target.Buffs |= Item.Parameter;
				SetItemStatusDuration(Context, User, Slot, 5, Item.Parameter, 5);
				BattleSteps::SetStatusMessage(Context, 5, Item.Parameter);
			}
			if ((Effect & 0x400) != 0)
			{
				Target.EtherBuffs |= Item.Parameter;
				SetItemStatusDuration(Context, User, Slot, 7, Item.Parameter, 5);
				BattleSteps::SetStatusMessage(Context, 7, Item.Parameter);
			}
			if ((Effect & 0x200) != 0)
			{
				const uint16 Before = Target.Elements;
				Target.Elements = Before | Item.Parameter;
				if ((Item.Parameter & 0xF000) != 0 && (Target.PermanentElements & 0xF000) == 0)
				{
					Target.Elements = static_cast<uint16>(((Before | Item.Parameter) & 0x0FFF) | Item.Parameter);
					SetItemStatusDuration(Context, User, Slot, 9, Item.Parameter, 5);
					BattleSteps::SetStatusMessage(Context, 9, Item.Parameter);
				}
				if ((Item.Parameter & 0x0F00) != 0 && (Target.PermanentElements & 0x0F00) == 0)
				{
					Target.Elements = static_cast<uint16>((Target.Elements & 0xF0FF) | Item.Parameter);
					SetItemStatusDuration(Context, User, Slot, 9, Item.Parameter, 5);
					BattleSteps::SetStatusMessage(Context, 9, Item.Parameter);
				}
			}
			// Igissol and Aegissol carry their immunities two bits too high: 80h (without Play Dead) and 40h.
			uint16 Immunities = Effect & 0x30;
			if (Context.RuleOptions.bImmunityItemsWork)
			{
				if ((Effect & 0x80) != 0 && (Item.Parameter & PlayDead) == 0)
				{
					Immunities |= 0x20;
				}
				if ((Effect & 0x40) != 0)
				{
					Immunities |= 0x10;
				}
			}
			if ((Immunities & 0x20) != 0)
			{
				Target.StatusImmunity |= 0x3F7C;
			}
			if ((Immunities & 0x10) != 0)
			{
				Target.StatusImmunity |= 0xFFFE;
			}
			if ((Effect & 8) != 0)
			{
				if (Item.Power > LastClearableWord)
				{
					Context.Fault = EBattleFault::StatusWordOutsideRecord;
					return;
				}
				Target.StatusWord(0x84 + 2u * Item.Power) = 0;
				switch (Item.Power)
				{
				case 0: Action.Message = 0x35; break;
				case 2: Action.Message = 0x36; break;
				case 4: Action.Message = 0x37; break;
				default: break;
				}
			}
			if ((Effect & 0x80) != 0 && (Item.Parameter & PlayDead) != 0)
			{
				if (!FBattleContext::IsParty(Slot))
				{
					Action.Message = 0x33;
					return;
				}
				if (Item.Setting == 0)
				{
					if (BattleSteps::CanPlayDead(Context))
					{
						Target.Status |= Item.Parameter;
						Action.Message = 0x31;
						Target.DisabledCommands = 0xFFEF;
					}
					else
					{
						Action.Message = 0x32;
					}
				}
				else
				{
					Target.Status &= static_cast<uint16>(~Item.Parameter);
					Target.DisabledCommands = Context.StartingDisabledCommands[Slot];
				}
			}
			if (Effect == 1)
			{
				ApplySpecial(Context, User, Slot, Item);
			}
		}
	}

	void PerformBattleItem(FBattleContext& Context, uint8 User, uint16 Targets, uint8 Item)
	{
		Context.Action.KnockedOut = 0;
		Context.Action.LivingAtStart = Context.Living;
		Context.Action.Targets = Targets;
		UseBattleItem(Context, User, Item);
	}

	void UseBattleItem(FBattleContext& Context, uint8 User, uint8 Item)
	{
		FBattleAction& Action = Context.Action;
		Action.Amounts = {};
		Action.Results.fill(BattleResults::NoResult);
		if (Item >= Context.Items.size())
		{
			Context.Fault = EBattleFault::CommandOutsideTable;
			return;
		}
		const FBattleItem& Record = Context.Items[Item];
		for (uint8 Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
		{
			if ((Action.Targets & SlotBit(Slot)) != 0)
			{
				ApplyItem(Context, User, Slot, Record);
			}
		}
		Action.Animation = Record.Animation;
		if ((Action.Revived & NoEffectInGear) != 0)
		{
			Action.Animation = NoEffectInGearAnimation;
		}
	}
}
