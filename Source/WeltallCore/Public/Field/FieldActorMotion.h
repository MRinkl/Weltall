#pragma once

#include "Field/EventContext.h"
#include "Foundation/Types.h"

namespace Weltall
{
	/**
	 * Plays one of the field's animation numbers on an actor, as the field does whenever an actor's
	 * animation changes: FF stands for animation 0, and a jump goes on only while the jump's animation
	 * plays (outside the player's action mode, only when that is animation 3). An actor with a model is
	 * left alone, and so is a sprite without animations; a gear's model belongs to the gears' module
	 * (a fault).
	 */
	WELTALLCORE_API void PlayActorAnimation(FEventContext& Context, int32 Actor, int32 Animation);

	/**
	 * Sets the velocity of the actor's sprite for walking along Direction (4096 a turn; bit 15 set:
	 * standing, no velocity). A sprite actor walks at its sprite's own speed, or, when its movement
	 * drives its sprite's velocity, at the actor's speed; an actor with a model or a gear walks at
	 * the actor's speed (a gear twice as fast) times its scale. The velocity is kept to whole 16ths of
	 * a unit. A gear its model moves gets its motion from the gears' module (a fault).
	 */
	WELTALLCORE_API void SetActorWalkVelocity(FEventContext& Context, int32 Actor, int32 Direction);

	/**
	 * Whether the actor stands still where it is and needs no new floor this tick: it has not moved,
	 * stands on no slope and no other actor, is neither jumping nor falling nor newly placed, and is on
	 * a layer it does not stay out of, and the map is past its first tick. (The original answers 0 for
	 * a settled actor and -1 otherwise.)
	 */
	WELTALLCORE_API bool IsActorSettled(const FEventActorState& State, const FFieldMovement& Movement);

	/** The horizontal distance between two actors, from their whole positions, as the field measures it. */
	WELTALLCORE_API int32 ActorGroundDistance(const FEventContext& Context, int32 From, int32 To);
}
