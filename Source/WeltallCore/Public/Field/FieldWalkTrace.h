#pragma once

#include "Field/EventContext.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"

#include <array>

namespace Weltall
{
	/** A walkmesh edge a move ran into, from its first corner to its second. */
	struct FWalkEdge
	{
		FGteVector From{};
		FGteVector To{};
	};

	/** Which floors a trace lets an actor onto. */
	enum class EWalkRule : uint8
	{
		/**
		 * The air's rule: closed are floors of a collision class the actor has (attribute bits 3-4
		 * against its classes 9-10, bits 5-7 against 8-10) and, on layer 0, floors marked 0x800000.
		 */
		Air,
		/**
		 * The ground's rule: floors of a class the actor has (bits 5-7 only), 0x800000 on layer 0,
		 * and a steep floor (0x400000) whose plane at the point rises above the actor's feet, unless
		 * the actor already stands on a steep floor.
		 */
		Ground,
	};

	/**
	 * What a trace leaves, as the original leaves its results in its caller's variables: each is
	 * written only where the original writes it (never from a wall's side of an edge), so a caller
	 * may see what an earlier trace left.
	 */
	struct FWalkTrace
	{
		/** The point traced to: x, the floor's height under it once a full trace reaches it, z. */
		FGteVector Point{};
		/** The edge a stopped trace crossed last, when it stopped on crossing one. */
		FWalkEdge Edge;
		/** Ground rule: the attribute word of the last triangle it tested, as the actor sees attributes. */
		uint32 Attributes = 0;
	};

	/**
	 * Traces a move of the actor by Delta (16.16) across its layer's triangles, from the one it stands
	 * on toward its position plus Delta, as the field does: at most 32 triangles, through neighbours
	 * only, every triangle entered tested by Rule. True when the point is reached. A probe only asks
	 * whether it is (the floor's height stays unset). The actor's layer and triangle must be in the
	 * walkmesh; a triangle of -1 stops the trace at once.
	 */
	WELTALLCORE_API bool TraceWalk(const FEventContext& Context, const FEventActorState& State, const std::array<int32, 3>& Delta, EWalkRule Rule, bool bProbe, FWalkTrace& Trace);

	/**
	 * Turns a move into a wall (Edge) into a slide along it, as the field does: a move within 11.25
	 * degrees of head-on stops; any other follows the wall, toward the side it leans to, at the move's
	 * full horizontal length (Delta's y becomes 0).
	 */
	WELTALLCORE_API void SlideAlongEdge(const FEventContext& Context, int32 Direction, const FWalkEdge& Edge, std::array<int32, 3>& Delta);

	/**
	 * Moves an actor standing on the ground by Delta, as the field does: three probes 4 units past
	 * the move (at its direction less 22.5 degrees, plus 22.5, then straight on), a slide along
	 * the wall the first to fail meets, then the move traced; a step up, a gentle step down, a
	 * climb or descent of a slope, and a ladder's floor are followed along the floor's rise at the
	 * move's horizontal length (the actor is marked as moved along a slope), while a drop of 64 units
	 * or more, or a slope entered from flat ground, is taken as it is. False when the move is blocked;
	 * otherwise Delta becomes the move made, its y reaching the new floor, and the actor's floor
	 * height the new floor's. Edge holds the edge a slide reads, and whatever a trace crossed last.
	 */
	WELTALLCORE_API bool MoveOnGround(const FEventContext& Context, FEventActorState& State, std::array<int32, 3>& Delta, int32 Direction, FWalkEdge& Edge);

	/**
	 * Moves an airborne actor, or one riding another, by Delta, as the field does: probes and a slide
	 * as on the ground (straight on first) under the air's rule, then the move traced; it lands only
	 * on a floor at or below its feet, or on its fixed floor height when it has one. False when the
	 * move is blocked; otherwise as MoveOnGround.
	 */
	WELTALLCORE_API bool MoveThroughAir(const FEventContext& Context, FEventActorState& State, std::array<int32, 3>& Delta, int32 Direction, FWalkEdge& Edge);

	/** The velocity (16.16) of a floor pushing at Speed toward Direction (4096 a turn), scaled by the map's push scale. */
	WELTALLCORE_API std::array<int32, 3> FloorPushVelocity(const FFieldMovement& Movement, int32 Speed, int32 Direction);
}
