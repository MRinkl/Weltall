#pragma once

#include "Foundation/Types.h"
#include "Support/OracleDigest.h"

#include <optional>
#include <vector>

namespace Weltall
{
	class FBattleRuntime;
	class FGameState;
	class FPsxRandom;
	struct FBattleContext;
}

namespace Weltall::Testing
{
	/** The buttons the Battle program oracles' pads hold, as FPadState numbers them: mostly Cross, the attack buttons, rarely the rest and Start. */
	std::vector<uint8> BattlePadButtons();

	/**
	 * A frame of the Battle program as its oracles digest it (less what the show was handed):
	 * the battle's state, the AI's, the party turn's and the event's, the actor's glow, the acting turns, the input's mode and
	 * pause, the battle's start, the mode's result and each slot's facing.
	 */
	void DigestBattleProgramState(FOracleDigest& Digest, const FBattleRuntime& Runtime, const FGameState& Game, const FPsxRandom& Random, bool bStarted,
		std::optional<bool> ListCursorShown = std::nullopt);

	/** The result's routine and its screen, once the result has begun: what the oracle's harness digests after the calls of a frame. */
	void DigestBattleResultRun(FOracleDigest& Digest, const FBattleContext& Context);
}
