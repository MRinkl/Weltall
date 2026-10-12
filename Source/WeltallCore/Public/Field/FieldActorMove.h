#pragma once

#include "Field/EventContext.h"
#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <optional>

namespace Weltall
{
	/**
	 * What its floor does to a walking actor this tick, added to its velocity (16.16), as the field does
	 * before the actor moves: a pushing floor pushes it (one kind of push on the ground, another in the
	 * air), a slope slides it down at a speed that grows with its gravity (on a steep slope it also
	 * loses its walking direction), and a platform it rides carries it along and round as the platform
	 * moves and turns. The floor's attributes are those its turn took; a gear its model moves is moved
	 * by the gears' module (a fault).
	 */
	WELTALLCORE_API void ApplyFloorForces(FEventContext& Context, int32 Actor);

	/**
	 * Works out a walking actor's move this tick, as the field does: its walking or running velocity
	 * along its direction and the velocity its floor gave it, the move traced on the ground or through
	 * the air (a party leader is kept out of floors closed to the others), the jump's speed, the
	 * animation its gait, jump, ladder or script asks for, slow ground halving it. The move is left as
	 * the actor's step for the layer choice to apply; a blocked actor stops, its velocities cleared.
	 */
	WELTALLCORE_API void MoveActor(FEventContext& Context, int32 Actor);

	/**
	 * One actor's turn in the field's first pass over the actors each tick, after the scripts: a free
	 * walking actor on screen takes its floor's attributes, its floor's forces and its move; a frozen
	 * one only takes the animation its script asks for. The error names what the port cannot move:
	 * a walking actor without a sprite or a floor, or a gear.
	 */
	[[nodiscard]] WELTALLCORE_API std::optional<FError> MoveFieldActor(FEventContext& Context, int32 Actor);
}
