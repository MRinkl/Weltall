#pragma once

#include "Field/EventActorState.h"
#include "Foundation/Types.h"

namespace Weltall
{
	/**
	 * Whether the ground point (X, Z) lies in an actor's box grown by Margin on every side: the
	 * rectangle of its half width and half depth around its integer position, as the field tests an
	 * actor with box collision. The test runs the GTE's NCLIP on points packed as the walkmesh's are,
	 * so a point on an edge is inside.
	 */
	WELTALLCORE_API bool IsInActorBox(const FEventActorState& Actor, int32 X, int32 Z, int32 Margin);
}
