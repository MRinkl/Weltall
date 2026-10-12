#pragma once

#include "Foundation/Types.h"
#include "Support/XorShift32.h"

#include <array>
#include <utility>

namespace Weltall::Testing::BattleAiWords
{
	/** The random enemy scripts' vocabulary, shared by BattleAiWords.cpp and BattleAiScripts.cpp. */
	inline constexpr uint32 FieldCount = 24;
	inline constexpr uint32 EnemyCommands = 98;

	using FWord = std::array<uint8, 4>;

	inline FWord Word(uint32 Op, uint32 A = 0, uint32 B = 0, uint32 C = 0)
	{
		return {static_cast<uint8>(Op), static_cast<uint8>(A), static_cast<uint8>(B), static_cast<uint8>(C)};
	}

	/** A variable of the enemy's own (a transformation hands one enemy's scripts to another). */
	inline uint32 Bv(FXorShift32& G, uint32) { return G.Next() % 16; }
	inline uint32 Wv(FXorShift32& G, uint32) { return G.Next() % 8; }
	inline uint32 Lv(FXorShift32& G, uint32) { return G.Next() % 4; }

	/** A word field a script may write: every one but the status and the gear's buffs. */
	uint32 WritableWordField(FXorShift32& G);

	/** A condition word, as battle_ai_scenario.condition draws it. */
	FWord Condition(FXorShift32& G, uint32 E);

	/** A command of every opcode the scripts use freely, as plain draws it. */
	FWord PlainCommand(FXorShift32& G, uint32 E, bool bEventWork);

	/** A command naming a slot by a mask, and the mask's own W, as guarded draws it. */
	std::pair<FWord, uint32> GuardedCommand(FXorShift32& G, uint32 E);
}
