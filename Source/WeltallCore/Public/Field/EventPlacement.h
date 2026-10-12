#pragma once

#include "Field/EventActorState.h"
#include "Field/FieldActor.h"
#include "Field/FieldWalkmesh.h"
#include "Foundation/Types.h"

#include <optional>

namespace Weltall
{
	/**
	 * The attribute word of the floor triangle the actor stands on, or 0 when it ignores its layer's
	 * attributes; nullopt when its layer, triangle or the triangle's attribute is not in the walkmesh.
	 */
	WELTALLCORE_API std::optional<uint32> ActorFloorAttributes(const FEventActorState& State, const FFieldWalkmesh& Walkmesh);

	/**
	 * Puts a newly reset script actor on the floor under its definition's position, as the field does
	 * when it creates the map's actors: its triangle on every walking layer, its floor's attributes and
	 * normal, and its height (unless its kind keeps the definition's). False when the walkmesh does not
	 * hold what the actor's layer needs.
	 */
	WELTALLCORE_API bool PlaceActorOnLoad(FEventActorState& State, FFieldActor& Record, const FFieldWalkmesh& Walkmesh);

	/**
	 * Moves a script actor to the ground point (X, Z) of its layer, on the floor, at rest: its
	 * triangles, attributes, normal and height, its record's position, motion cleared and its
	 * interaction point back on it. An actor on the camera's layer, or on one of the four layers the map
	 * does not have, has no floor to stand on: it is placed at height 0 with no floor normal. False when
	 * the walkmesh does not hold what the actor's layer needs.
	 */
	WELTALLCORE_API bool PlaceActor(FEventActorState& State, FFieldActor& Record, const FFieldWalkmesh& Walkmesh, int32 X, int32 Z);
}
