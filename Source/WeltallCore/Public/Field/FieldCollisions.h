#pragma once

#include "Field/EventContext.h"
#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/GteVectors.h"

#include <optional>

namespace Weltall
{
	/** The floor a platform gives under a point: the highest of its polygons there, and the normal of the last found. */
	struct FPlatformFloor
	{
		int32 Height = 0;
		FGteLongVector Normal{};
	};

	/**
	 * The floor a platform actor's model gives at the ground point (X, Z), as the field finds it: the
	 * model's polygons (with the vertices a MIMe animation left them), placed as the model is (turned by
	 * the actor's extra rotation, or by the world's rotation and, when it is attached to another actor,
	 * by that actor's place in the world as the scenery's last update left it), that face up and hold the
	 * point from above; the highest of their planes there. Nothing when none holds it. A platform
	 * without a model, or attached to an actor the map does not have, is a fault.
	 */
	WELTALLCORE_API std::optional<FPlatformFloor> PlatformFloor(FEventContext& Context, int32 Platform, int32 X, int32 Z);

	/**
	 * The controlled actor against the others, once per tick after the actors' moves, as the field does:
	 * where its step takes it, every other actor that is not frozen is met as a cylinder (its radius), a
	 * box, or a platform's polygons. Unless either passes through actors or the party walks as a group,
	 * an actor met at the controlled actor's height blocks it (a pushable one is pushed once it has been
	 * pushed against long enough); one whose top is just below its feet, or a platform under it, is stood
	 * on and carries it. Then the controlled actor takes its layer and falls, and a landing its sprite
	 * signals drops the pad's queued input and ends its jump.
	 */
	[[nodiscard]] WELTALLCORE_API std::optional<FError> CollideControlledActor(FEventContext& Context);
}
