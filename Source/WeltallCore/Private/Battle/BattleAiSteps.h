#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	struct FBattleContext;
}

namespace Weltall::BattleAiSteps
{
	/** A script word: its opcode and three operand bytes. */
	struct FWord
	{
		uint8 Op = 0;
		uint8 A = 0;
		uint8 B = 0;
		uint8 C = 0;

		/** The last two bytes as one little-endian number. */
		uint16 Immediate() const { return static_cast<uint16>(B | C << 8); }
	};

	/** A command word (opcode below 80h) of enemy Enemy's script; returns the queue's next entry. */
	uint8 RunCommand(FBattleContext& Context, uint8 Enemy, const FWord& Word, uint8 Next);

	/** The target pickers, counts and masks (40h-61h, 64h-68h); false for any other opcode. */
	bool RunPicker(FBattleContext& Context, uint8 Enemy, const FWord& Word);

	/** A single condition word's truth (81h-98h, 9Bh; every other word is false). */
	bool Condition(FBattleContext& Context, uint8 Enemy, const FWord& Word);

	/** One entry of the action queue carried out for an actor; false when the entry is empty (type 0). */
	bool ExecuteEntry(FBattleContext& Context, uint8 Actor, uint8 Entry);
}
