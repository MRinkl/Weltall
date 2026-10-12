#pragma once

#include "Battle/BattleContext.h"
#include "Battle/BattleEvent.h"

#include <cstddef>
#include <initializer_list>

namespace Weltall::BattleEventSteps
{
	/** A handler's answer when the instruction waits for a frame inside itself; otherwise its length (0 waits). */
	inline constexpr int32 TakeFrame = -1;
	inline constexpr uint8 Free = FBattleEventRecord::Free;

	/** The code's byte or little-endian word at an offset from the code's start; past the script faults. */
	uint8 CodeByte(FBattleContext& Context, std::size_t Offset);
	uint16 CodeWord(FBattleContext& Context, std::size_t Offset);

	/** An actor's entry point, read as the game reads it (past the actor's own, the next actor's or the code). */
	uint16 Entry(FBattleContext& Context, uint32 Actor, uint32 Script);

	/** The variable at a byte address of the work's variables (bit 0 ignored); past them faults. */
	uint16* Variable(FBattleContext& Context, uint32 Address);

	/** Word operands (bit 15 a number, else a variable's address) into the work's operands. */
	void FetchWords(FBattleContext& Context, std::size_t Pc, int32 Count);

	/** Operands with a flag byte: operand i a number when bit 80h >> i is set, else a variable's address. */
	void FetchFlagged(FBattleContext& Context, std::size_t Pc, int32 Count, uint8 Flags);

	/** The combatant slot of a character (the party slot holding it, 0 when none) or an enemy (value - 0Dh). */
	uint8 CombatantSlot(const FBattleContext& Context, uint16 Value);

	/** A combatant slot the show can act on; past the battle's eleven faults. */
	bool IsShownSlot(FBattleContext& Context, uint32 Slot);

	void Cue(FBattleContext& Context, EBattleEventCue Kind, std::initializer_list<uint16> Args);

	bool ShowBlocks(const FBattleContext& Context);

	/** The instructions from Message (18h) on, which act on the show, the combatants and the battle. */
	int32 ExecuteBattleOp(FBattleContext& Context, uint8 Actor, uint8 Opcode, std::size_t Pc);
}
