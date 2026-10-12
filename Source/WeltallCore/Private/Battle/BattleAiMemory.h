#pragma once

#include "Battle/BattleContext.h"

#include <cstddef>

namespace Weltall::BattleAiMemory
{
	/**
	 * An enemy's variables as its scripts address them: B[n] at the record's byte 30h + n, W[n] at
	 * 20h + 2n, L[n] at 10h + 4n. The index is a whole byte, so a large one reaches the next enemies'
	 * records as in the game; one past the eighth record, or over a record's scripts, faults (0 read).
	 */
	uint8 B(FBattleContext& Context, uint8 Enemy, uint8 Index);
	void SetB(FBattleContext& Context, uint8 Enemy, uint8 Index, uint8 Value);
	uint16 W(FBattleContext& Context, uint8 Enemy, uint8 Index);
	void SetW(FBattleContext& Context, uint8 Enemy, uint8 Index, uint16 Value);
	uint32 L(FBattleContext& Context, uint8 Enemy, uint8 Index);
	void SetL(FBattleContext& Context, uint8 Enemy, uint8 Index, uint32 Value);

	/** The scripts' globals, kept by the game from battle to battle; an index past the 16 faults. */
	uint16 Global(FBattleContext& Context, uint8 Index);
	void SetGlobal(FBattleContext& Context, uint8 Index, uint16 Value);

	/** The first slot of a mask, 11 when it has none of the eleven. */
	uint8 FirstSlot(uint16 Mask);

	/** A combatant's byte and word fields by the number the scripts give them; slot 11 or a number past the table faults. */
	uint8 ByteField(FBattleContext& Context, uint8 Slot, uint8 Field);
	void SetByteField(FBattleContext& Context, uint8 Slot, uint8 Field, uint8 Value);
	uint16 WordField(FBattleContext& Context, uint8 Slot, uint8 Field);
	void SetWordField(FBattleContext& Context, uint8 Slot, uint8 Field, uint16 Value);

	/** The stage group a slot stands in; slot 11 faults. */
	uint8 Group(FBattleContext& Context, uint8 Slot);

	/** In the battle, shown, not knocked out, out or playing dead, and without the pickers' exemption unless allowed. */
	bool IsTargetable(const FBattleContext& Context, uint8 Slot, uint8 bExemptAllowed);
	/** The same whether shown or not. */
	bool IsTargetableOrHidden(const FBattleContext& Context, uint8 Slot, uint8 bExemptAllowed);
	/** In the battle, shown, not knocked out or out. */
	bool IsPresent(const FBattleContext& Context, uint8 Slot);
	/** Fights in a gear, as the AI's pickers tell. */
	bool InGear(const FBattleContext& Context, uint8 Slot);

	/** The queue's byte at Offset (entry × 8 + field); past the queue faults. */
	uint8 QueueByte(FBattleContext& Context, std::size_t Offset);
	void SetQueueByte(FBattleContext& Context, std::size_t Offset, uint8 Value);
	uint16 QueueWord(FBattleContext& Context, std::size_t Offset);
}
