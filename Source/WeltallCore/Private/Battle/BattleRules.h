#pragma once

#include "Battle/BattleContext.h"
#include "Foundation/ProcessorDivide.h"
#include "Game/PsxRandom.h"

namespace Weltall::BattleRules
{
	/** The combatant a record pointer of the rules points at; one never set faults (the game reads low memory). */
	inline FBattleCombatant& Combatant(FBattleContext& Context, uint8 Index)
	{
		if (Index >= FBattleContext::SlotCount)
		{
			Context.Fault = EBattleFault::UnsetRecord;
			return Context.Combatants[0];
		}
		return Context.Combatants[Index];
	}

	inline FBattleCombatant& Actor(FBattleContext& Context)
	{
		return Combatant(Context, Context.Action.ActorCombatant);
	}

	inline FBattleCombatant& Target(FBattleContext& Context)
	{
		return Combatant(Context, Context.Action.TargetCombatant);
	}

	inline FBattleGearPart& ActorGear(FBattleContext& Context)
	{
		return Combatant(Context, Context.Action.ActorGear).GearPart;
	}

	inline FBattleGearPart& TargetGear(FBattleContext& Context)
	{
		return Combatant(Context, Context.Action.TargetGear).GearPart;
	}

	/** A command record; one the tables did not fill faults. */
	inline FBattleCommand& CommandAt(FBattleContext& Context, int32 Index)
	{
		if (!FBattleContext::IsLoadedCommand(Index))
		{
			Context.Fault = EBattleFault::CommandOutsideTable;
			return Context.Commands[0];
		}
		return Context.Commands[static_cast<std::size_t>(Index)];
	}

	/** The command the rules point at. */
	inline FBattleCommand& Command(FBattleContext& Context)
	{
		return CommandAt(Context, Context.Action.CommandIndex);
	}

	inline bool InGear(const FBattleContext& Context, int32 Slot)
	{
		return Context.Combatants[static_cast<std::size_t>(Slot)].IsInGear();
	}

	inline uint16 SlotBit(int32 Slot)
	{
		return static_cast<uint16>(1u << Slot);
	}

	/**
	 * A slot's bit as the game looks it up in its table of the sixteen bits, which the table of the high
	 * bits (8000h down) follows: 0 for no slot (FFh, a zero word of the data there); other numbers past
	 * both tables fault.
	 */
	inline uint16 GameSlotBit(FBattleContext& Context, uint8 Slot)
	{
		if (Slot < 16)
		{
			return static_cast<uint16>(1u << Slot);
		}
		if (Slot < 32)
		{
			return static_cast<uint16>(0x8000u >> (Slot - 16));
		}
		if (Slot != 0xFF)
		{
			Context.Fault = EBattleFault::IndexOutsideTable;
		}
		return 0;
	}

	/** The game's rand(), 0 to 7FFFh. */
	inline int32 Draw(FBattleContext& Context)
	{
		return Context.Random->Next();
	}

	/** rand() % 100, as the rules roll their chances. */
	inline int32 Percent(FBattleContext& Context)
	{
		return Draw(Context) % 100;
	}

	/**
	 * The battle's number from Low to High: FFFFh when Low is, 0 when High is, Low when they are equal
	 * (none of these draws), else Low plus the next number modulo the span (the next number itself for
	 * the widest span, so nothing above 7FFFh comes).
	 */
	inline uint16 RandomU16Between(FPsxRandom& Random, uint16 Low, uint16 High)
	{
		if (Low == 0xFFFF)
		{
			return 0xFFFF;
		}
		if (High == 0)
		{
			return 0;
		}
		if (Low == High)
		{
			return Low;
		}
		const int32 Span = static_cast<int32>(High) - static_cast<int32>(Low);
		if (Span < 0xFFFF)
		{
			return static_cast<uint16>(Low + ProcessorRemainder(Random.Next() & 0xFFFF, Span + 1));
		}
		return static_cast<uint16>(Random.Next());
	}

	/** Points the actor's records at a slot's combatant, as the rules do before reading an actor. */
	inline void PointActorAt(FBattleContext& Context, int32 Slot)
	{
		Context.Action.ActorCombatant = static_cast<uint8>(Slot);
		Context.Action.ActorGear = static_cast<uint8>(Slot);
	}

	/** Points the target's records at a slot's combatant. */
	inline void PointTargetAt(FBattleContext& Context, int32 Slot)
	{
		Context.Action.TargetCombatant = static_cast<uint8>(Slot);
		Context.Action.TargetGear = static_cast<uint8>(Slot);
		Context.Action.TargetFields = static_cast<uint8>(Slot);
	}
}
