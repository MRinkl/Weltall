#pragma once

#include "Foundation/Types.h"

#include <optional>

namespace Weltall
{
	struct FEventContext;

	/**
	 * A walking actor's layer and position step, after its move: the field runs it for every walking
	 * actor but the leader, and last in the leader's collisions (Height: the platform they found it
	 * standing on).
	 *
	 * The floor under the actor's position plus its step is traced on every walking layer from the
	 * actor's triangle there, and the floors are sorted by height. An actor above its own floor, or in
	 * the air, moves to the first layer whose floor lies at or below its feet; a floor with attribute
	 * bit 2 hands it to the layer above. When the new floor is open to the actor, no other layer's floor
	 * cuts through its body and a ceiling leaves its head room, it takes the step; otherwise the step is
	 * undone. Either way it falls under its sprite's gravity until it lands on its floor, and the leader's
	 * step goes into the party's trail. An undone step of the leader keeps the d-pad out of the next
	 * tick's buttons.
	 *
	 * Nothing happens for a party follower (whose step is the trail's), an actor placed or moved by the
	 * scripts, or one that has settled on its floor. False when the actor has no sprite, or its layer or
	 * triangles are not in the walkmesh.
	 */
	WELTALLCORE_API bool StepWalkLayer(FEventContext& Context, int32 Actor, std::optional<int32> PlatformHeight = std::nullopt);
}
