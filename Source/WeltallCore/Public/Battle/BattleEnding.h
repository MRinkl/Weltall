#pragma once

#include "Battle/BattleEndingState.h"
#include "Foundation/Types.h"

namespace Weltall
{
	struct FBattleContext;

	/**
	 * The battle's end, once its loop has stopped: the end sorted into the
	 * mode's result (the game state's ProgramResult) and the party's pose; a won battle or a scripted
	 * defeat runs the event once more (its variable 0 FFh, the party's knock-outs in 10h-12h, the
	 * result put aside while it runs), unless the event ended the battle without asking to run after it.
	 * Run it with StepBattleEnding.
	 */
	WELTALLCORE_API void BeginBattleEnding(FBattleContext& Context);

	/**
	 * The end to its next frame: the event's run, the loads finished, the pose, the dim behind the
	 * result screen, the event's own end and the music's fade after a defeat. False once it is over:
	 * the result comes next (FinishBattleResults with the mode's result).
	 */
	WELTALLCORE_API bool StepBattleEnding(FBattleContext& Context);

	/** What the battle leaves the programs after it, once its result is done. */
	WELTALLCORE_API FBattleOutcome TakeBattleOutcome(const FBattleContext& Context);
}
