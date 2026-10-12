#pragma once

#include "Battle/BattleContext.h"
#include "Game/GameState.h"
#include "Support/OracleDigest.h"
#include "Support/XorShift32.h"

namespace Weltall::Testing
{
	/** The result's part of a case the context does not hold: the field's mode result the result reads. */
	struct FBattleResultCase
	{
		uint8 ModeResult = 0;
	};

	/**
	 * The result's part of a random battle, drawn after DrawBattleState exactly as
	 * the oracle's generator draws it: the story flags, the characters' levels,
	 * EXP to the next, growth stats, learned skills and AP, Bart's gear, the party's statuses, EXP shares and
	 * counters, the enemies' deaths, rewards and drops, the special party, the formation's flag, the event's
	 * requests, the battle's result and the field's mode result, the EXP penalty, the items listed.
	 */
	FBattleResultCase DrawBattleResultState(FXorShift32& G, FBattleContext& Context, FGameState& State);

	/** The result's work in the order the oracle takes the original's memory (after the battle state). */
	void DigestBattleResultState(FOracleDigest& Digest, const FBattleContext& Context);
}
