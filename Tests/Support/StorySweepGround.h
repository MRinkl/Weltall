#pragma once

#include "Foundation/Types.h"
#include "Graphics/Gte.h"

#include <array>
#include <compare>
#include <functional>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace Weltall
{
	struct FEventActorState;
	struct FEventContext;
}

namespace Weltall::Testing
{
	/** Whether touching the actor starts a script of its own, as it stands: one that does more than end. */
	bool AnswersFieldTouch(const FEventContext& Context, int32 Actor);

	/** Whether an actor's body would stop the leader standing at (X, Z) with its feet at Feet, as the leader's collisions meet bodies. */
	bool StopsLeaderAt(const FEventActorState& Actor, const FEventActorState& Leader, int32 X, int32 Z, int32 Feet);

	/** A place a walk of the leader's sets off from: a ground point on the floor of one of the walking layers. */
	struct FFieldStanding
	{
		int32 Layer = 0;
		int32 X = 0;
		int32 Z = 0;
	};

	/** Where the leader stands. */
	FFieldStanding LeaderStanding(const FEventContext& Context);

	/**
	 * One of a map's floors as the leader walks them: the triangles of a walking layer joined edge to edge through
	 * triangles open to it, named by the layer and the lowest of them. A walk never leaves its floor; a jump, a drop
	 * off a ledge or a floor that hands the walker to the layer above may.
	 */
	struct FWalkedFloor
	{
		int32 Layer = -1;
		int32 First = -1;

		auto operator<=>(const FWalkedFloor&) const = default;
	};

	/** A floor under a ground point, and its height there (negative is up). */
	struct FFloorUnder
	{
		FWalkedFloor Floor;
		int32 Height = 0;
	};

	/** The floor of the layer's triangle holding the ground point; none where no triangle open to the leader holds it. */
	std::optional<FFloorUnder> WalkedFloorAt(const FEventContext& Context, int32 Layer, int32 X, int32 Z);

	/** How many walking layers the map has: all but the last, the camera's. */
	int32 WalkingLayers(const FEventContext& Context);

	/** The floor the leader stands on: its own triangle's. */
	std::optional<FWalkedFloor> LeaderFloor(const FEventContext& Context);

	/**
	 * The attribute records, of Records, of the triangles closed to the leader beside the floor it stands on: ways off
	 * that floor shut by a word the map's scripts change, gates they may open.
	 */
	std::set<int32> ShutGatesBeside(const FEventContext& Context, const std::set<int32>& Records);

	/** The middle of the triangles of the leader's layer whose attribute record is Record: where a gate stands. */
	std::pair<double, double> GateMiddle(const FEventContext& Context, int32 Record);

	/** A ground point a walk comes to, with the floor's height there: at the ground's rim (a neighbour it does not reach), on a steep slope (which no jump leaves). */
	struct FGroundPoint
	{
		int32 X = 0;
		int32 Z = 0;
		int32 Height = 0;
		bool bRim = false;
		bool bSteep = false;
	};

	/** A ground point a walk comes to, and the one it passed some way before it. */
	struct FGroundWay
	{
		int32 X = 0;
		int32 Z = 0;
		int32 FromX = 0;
		int32 FromZ = 0;
	};

	/** The map's floors of at least MinTriangles triangles, each with a standing on one of its triangles. */
	std::vector<std::pair<FWalkedFloor, FFieldStanding>> WalkedFloors(const FEventContext& Context, int32 MinTriangles);

	/**
	 * The ground a walk of the leader's reaches from a place on a floor (where it stands, unless another place is
	 * given) with nothing met on its way: points a few units apart on the floor, joined from the place's own through
	 * their neighbours. Left out: the points an actor's body stops the leader at, and those where another actor's touch
	 * would answer it, since a player who walks there has met that actor first. What holds the walk where it sets off
	 * already bars no way.
	 */
	class FWalkedGround
	{
	public:
		/**
		 * Target is the actor walked up to, whose own touch bars nothing (none: -1); Crossed the actors whose touch
		 * the walk has met before and may meet again. Without Crossed nothing bars the walk but the floor's own joins.
		 */
		FWalkedGround(const FEventContext& Context, int32 Target, const std::set<int32>* Crossed, const std::optional<FFieldStanding>& From = std::nullopt);

		/** Whether the walk reaches the ground point: on the floor it walks, at one of the ground's points it came to or within a step of one. */
		bool Reaches(int32 X, int32 Z) const;

		/** Whether the ground point lies at or beside a point the walk came to, seen from above: the floor under it may be another's. */
		bool Covers(int32 X, int32 Z) const;

		/** The points the walk comes to at the ground's rim, and of the rest each Every-th along both ways (none for 0). */
		std::vector<FGroundPoint> Points(int32 Every) const;

		/** The points the walk comes to on triangles that hand the walker to the layer above, each with its triangle. */
		const std::vector<std::array<int32, 3>>& PassesUp() const { return Upward; }

		/** The floor's height at a point the walk comes to. */
		std::optional<int32> HeightAt(int32 X, int32 Z) const;

		/** What stopped the walk beside a point it came to: an actor, by its body or (bTouch) by its touch not met before. */
		struct FBarrier
		{
			int32 Actor = -1;
			bool bTouch = false;

			auto operator<=>(const FBarrier&) const = default;
		};

		/** The actors that took floor from the walk between ground it reached and floor it did not, in order: what it would have to pass to go further. */
		const std::vector<FBarrier>& Barriers() const { return Stopped; }

		/** How many points the walk came to. */
		int32 Size() const { return static_cast<int32>(Order.size()); }

		/**
		 * The first point the walk comes to that Wanted holds for, the nearest by its own way, with the point about
		 * Back units before it on that way (where the walk sets off, on a shorter way). None where it holds for none.
		 */
		std::optional<FGroundWay> WayTo(const std::function<bool(int32 X, int32 Z)>& Wanted, int32 Back) const;

	private:
		/** The point's place among Reached; -1 outside the ground. */
		int32 PointAt(int32 X, int32 Z) const;

		int32 MinX = 0;
		int32 MinZ = 0;
		int32 Columns = 0;
		int32 Rows = 0;
		std::vector<uint8> Reached;
		/** The points the walk came to, the nearest first, and for each point the one the walk came to it from (itself where it sets off). */
		std::vector<int32> Order;
		std::vector<int32> Before;
		std::vector<FBarrier> Stopped;
		std::vector<int32> Heights;
		std::vector<uint8> Steep;
		/** The corners of the triangles the walk's floor is made of. */
		std::vector<std::array<FGteVector, 3>> Floors;
		std::vector<std::array<int32, 3>> Upward;
	};
}
