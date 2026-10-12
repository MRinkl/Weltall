#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	struct FBattleContext;

	/**
	 * One action as the rules carry it out: computed at once for every target, the results kept in the
	 * context's action, then committed into a record the show plays.
	 */

	/**
	 * Carries out a command: the actor, its number (in the actor's table, or the enemies'), the target
	 * mask, and the number the show plays; the knocked-out mask starts empty and the living are noted.
	 */
	WELTALLCORE_API void PerformBattleCommand(FBattleContext& Context, uint8 Actor, uint8 Command, uint16 Targets, uint16 Animation);

	/**
	 * The action the context's action was given: each target in slot order, a party target's counter
	 * first, the command's handler, what follows each target (the reactions of its equipment and
	 * statuses, a landed hit's status), then the actor's element for the show, Citan's party-wide
	 * ether, Billy's ammunition and the Deathblow counter a basic attack feeds.
	 */
	WELTALLCORE_API void ExecuteBattleAction(FBattleContext& Context);

	/**
	 * Puts the action's results into performance record Record, adds them to the turn's running
	 * totals, and applies them: HP, EP and fuel changed, the knocked out marked and moved out of their
	 * group.
	 */
	WELTALLCORE_API void CommitBattleResults(FBattleContext& Context, uint8 Record);

	/** Applies performance record Record's results alone. */
	WELTALLCORE_API void ApplyBattleResults(FBattleContext& Context, uint8 Record);

	/** Empties the current performance record's amounts, results and totals. */
	WELTALLCORE_API void ClearBattlePerformance(FBattleContext& Context);

	/** Empties the turn's running totals. */
	WELTALLCORE_API void ResetBattleTurnTotals(FBattleContext& Context);

	/**
	 * The route an approach walks from the actor's place to the target's group by the
	 * stage's routes (the context's Route); true when the actor is Billy, who shoots from where he stands.
	 */
	WELTALLCORE_API bool PlanBattleApproachPath(FBattleContext& Context, uint8 Actor, uint8 Target);
}
