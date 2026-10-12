#include "Battle/BattleEnd.h"

#include "Battle/BattleRules.h"

namespace Weltall
{
	using namespace BattleRules;

	namespace
	{
		constexpr uint16 KnockedOut = 0x8000;
		constexpr uint16 Out = 0xC000;
		constexpr uint16 Asleep = 0x1000;
		constexpr uint8 NeverAgain = 0xFF;
		constexpr uint16 EnemySlots = 0x7F8;
		constexpr uint16 PartySlots = 0x007;

		bool IsShown(const FBattleContext& Context, std::size_t Slot)
		{
			return Context.Slots[Slot].Presence == 0 && !Context.Escaped[Slot - FBattleContext::PartySlotCount];
		}
	}

	bool IsPartyDown(const FBattleContext& Context)
	{
		bool bAnyone = false;
		for (std::size_t Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
		{
			if (!Context.InBattle[Slot] || Context.Slots[Slot].Presence != 0)
			{
				continue;
			}
			if ((Context.Combatants[Slot].Status & Out) == 0)
			{
				return false;
			}
			bAnyone = true;
		}
		return bAnyone;
	}

	void CheckBattleEnd(FBattleContext& Context)
	{
		Context.Living = 0;
		for (std::size_t Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
		{
			if (!Context.InBattle[Slot])
			{
				continue;
			}
			FBattleCombatant& Member = Context.Combatants[Slot];
			if ((Member.Status & Out) == 0)
			{
				if (Context.Slots[Slot].Presence == 0)
				{
					Context.Living |= SlotBit(static_cast<int32>(Slot));
				}
			}
			else
			{
				if ((Member.Status & KnockedOut) != 0)
				{
					Member.Hp = 0;
				}
				Context.Ready[Slot] = NeverAgain;
			}
		}
		for (std::size_t Slot = FBattleContext::PartySlotCount; Slot < FBattleContext::SlotCount; ++Slot)
		{
			if (!Context.InBattle[Slot])
			{
				continue;
			}
			FBattleCombatant& Enemy = Context.Combatants[Slot];
			const uint16 Bit = SlotBit(static_cast<int32>(Slot));
			const bool bGear = Context.Slots[Slot].InGear != 0;
			const uint16 Status = bGear ? Enemy.GearPart.Status : Enemy.Status;
			if ((Status & Out) == 0)
			{
				if (IsShown(Context, Slot))
				{
					Context.Living |= Bit;
				}
			}
			else if ((Context.ReactingWhileKnockedOut & Bit) != 0)
			{
				Context.Living |= Bit;
			}
			else
			{
				if (bGear)
				{
					Enemy.GearPart.Hp = 0;
				}
				else
				{
					Enemy.Hp = 0;
				}
				Context.Ready[Slot] = NeverAgain;
			}
		}
		if ((Context.Living & EnemySlots) == 0)
		{
			Context.End = BattleEnd::Won;
		}
		if ((Context.Living & PartySlots) == 0)
		{
			Context.End = BattleEnd::Lost;
		}
		if (Context.End != BattleEnd::Running)
		{
			return;
		}
		uint16 Awake = Context.Living;
		for (std::size_t Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
		{
			if ((Context.Living & SlotBit(static_cast<int32>(Slot))) != 0 && (Context.Combatants[Slot].Mental & Asleep) != 0)
			{
				Awake = static_cast<uint16>(Awake & ~SlotBit(static_cast<int32>(Slot)));
			}
		}
		if (Awake != 0)
		{
			return;
		}
		for (std::size_t Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
		{
			FBattleCombatant& Sleeper = Context.Combatants[Slot];
			if ((Context.Living & SlotBit(static_cast<int32>(Slot))) != 0 && (Sleeper.Mental & Asleep) != 0)
			{
				Sleeper.Mental &= static_cast<uint16>(~Asleep);
				return;
			}
		}
	}
}
