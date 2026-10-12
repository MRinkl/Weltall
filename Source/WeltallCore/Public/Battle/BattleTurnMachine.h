#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	struct FBattleContext;

	/**
	 * A turn of the battle, frame by frame as the game runs it: the actor's statuses
	 * counted down; a party member's command input (or its automatic turn) and the reactions of the
	 * enemies it aimed at, or an enemy's turn script and actions; the action's name, the event's hooks;
	 * then the default targets chosen again, the end checked around the actor's drains, and its next wait.
	 * The waits stand still throughout. The actor is TurnActor's (a slot + 1); none (0) ends at once.
	 */
	WELTALLCORE_API void BeginBattleTurn(FBattleContext& Context, bool bScripted);

	/**
	 * The turn to its next frame, the frame's event (the input's reading) first. False once the turn has
	 * ended (the frame then belongs to the battle's loop, the waits running again).
	 */
	WELTALLCORE_API bool StepBattleTurn(FBattleContext& Context, uint8 Event);
}
