#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	struct FBattleContext;

	/**
	 * The waits that decide who acts next. Each slot counts its wait down every step while no turn
	 * runs; one at 0 is ready, and the ready act one at a time in the battle's random order.
	 */

	/**
	 * A slot's next wait (one draw), from its agility less the speed of the command it last used (a
	 * party slot's) or its agility alone (an enemy's); also ends Play Dead when no one else could act.
	 */
	WELTALLCORE_API uint8 RollBattleWait(FBattleContext& Context, int32 Slot);

	/** Every slot in the battle gets a first wait (the others FFh); nobody is ready. */
	WELTALLCORE_API void InitBattleWaits(FBattleContext& Context);

	/** One step of the waits: each slot in the battle and not ready counts down, at its statuses' pace. */
	WELTALLCORE_API void TickBattleWaits(FBattleContext& Context);

	/**
	 * Who takes the next turn: a slot an enemy's script forces, else the first ready slot from where
	 * the battle's order stopped; TurnActor is that slot + 1, or 0 when nobody is ready.
	 */
	WELTALLCORE_API void PickBattleActor(FBattleContext& Context);

	/** After a turn: the actor, unless it never acts again, waits anew from the command it used. */
	WELTALLCORE_API void RollActorWait(FBattleContext& Context);

	/** After a turn: a party member the turn revived waits its whole last wait again. */
	WELTALLCORE_API void ResetRevivedWaits(FBattleContext& Context);

	/** Play Dead ends for every member when it would leave no member able to act. */
	WELTALLCORE_API void EndPlayDeadIfAlone(FBattleContext& Context);
}
