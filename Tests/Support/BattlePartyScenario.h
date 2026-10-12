#pragma once

#include "Battle/BattleContext.h"
#include "Game/GameState.h"
#include "Support/OracleDigest.h"
#include "Support/XorShift32.h"

#include <vector>

namespace Weltall::Testing
{
	/** A party turn to run: who acts, whether by itself, each frame's press and the frames the show takes to play the actions. */
	struct FBattlePartyCase
	{
		uint8 Actor = 0;
		bool bAuto = false;
		std::vector<uint8> Script;
		uint8 PlaybackFrames = 0;
		/** The actor's default target is the one chosen before the turn (else the last of its candidates). */
		bool bPicksTarget = true;
	};

	/**
	 * The party turn's part of a random battle state, drawn after DrawBattleState and DrawBattleAiState exactly as
	 * the oracle's generator draws it, with its overrides of theirs.
	 */
	FBattlePartyCase DrawBattlePartyState(FXorShift32& G, FBattleContext& Context, FGameState& State);

	/** The party turn's part in the order the oracle takes the original's memory. */
	void DigestBattlePartyState(FOracleDigest& Digest, const FBattleContext& Context);
}
