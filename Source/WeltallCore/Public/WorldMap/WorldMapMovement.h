#pragma once

#include "Foundation/Types.h"
#include "WorldMap/WorldMapTerrain.h"

#include <array>

namespace Weltall
{
	struct FGteArctangents;

	/** The travel modes, as the walk table's rows. */
	namespace WorldMapTravel
	{
		inline constexpr int32 OnFoot = 1;
		inline constexpr int32 Gears = 2;
		inline constexpr int32 SandShip = 4;
		inline constexpr int32 SeaShip = 5;
		inline constexpr int32 Flying = 7;
	}

	/** How a move meets the cells it crosses. */
	enum class EWorldMapCrossing : uint8
	{
		/** Nothing blocks it. */
		Free = 0,
		/** Blocked at a point (Stop): the move slides along that cell's split line. */
		Stopped = 1,
		/** The next cell across a z boundary is blocked: the move slides along x. */
		BlockedAcrossZ = 2,
		/** The next cell across an x boundary is blocked: the move slides along z. */
		BlockedAcrossX = 3,
	};

	/**
	 * Where a move of Speed (20.12 units a tick) along Direction (4.12) from Position meets ground the
	 * travel mode may not cross, the cell boundaries it crosses tested in the order the program tests
	 * them. Stop is the point a Stopped move halts at (x and z; the rest as it was).
	 */
	WELTALLCORE_API EWorldMapCrossing CheckWorldMapCellCrossing(const FWorldMapGround& Ground, const FWorldMapVector& Position,
		const FWorldMapVector& Direction, int32 Speed, int32 TravelMode, FWorldMapVector& Stop);

	/**
	 * The terrain's part of a move: true when the move is free, Result keeping
	 * the destination the caller put there; else Result is the way to try instead, along an axis or
	 * a split line (its y then left as it was), or nothing.
	 */
	WELTALLCORE_API bool WorldMapTerrainStep(const FWorldMapGround& Ground, const FWorldMapVector& Position, const FWorldMapVector& Direction,
		FWorldMapVector& Result, int32 Speed, int32 TravelMode);

	/**
	 * A move over the ground where no place model collides, as in every
	 * playable scene: Result's x and z the wrapped destination, its y the ground's there less 4 units,
	 * then the terrain's step. Result's fourth word is left as it was.
	 */
	WELTALLCORE_API bool MoveOnWorldMapGround(const FWorldMapGround& Ground, const FWorldMapVector& Position, const FWorldMapVector& Direction,
		FWorldMapVector& Result, int32 Speed, int32 TravelMode);

	/**
	 * A move through the air: as over the ground in three dimensions, the height
	 * kept at least 32 units above the ground (and at most 32 below sea level) and at most 640 up.
	 */
	WELTALLCORE_API bool MoveInWorldMapAir(const FWorldMapGround& Ground, const FWorldMapVector& Position, const FWorldMapVector& Direction,
		FWorldMapVector& Result, int32 Speed, int32 TravelMode);

	/**
	 * A scripted walk's step toward a point: each axis moves by half the speed
	 * along the direction until within 4 units of the target's; the height follows the ground. Bit 0
	 * of the result: x has arrived; bit 1: z has.
	 */
	WELTALLCORE_API int32 WalkTowardWorldMapPoint(const FWorldMapGround& Ground, FWorldMapVector& Position, const FWorldMapVector& Direction,
		int32 Speed, int32 TargetX, int32 TargetZ);

	/** The heading (0-FFFh) from one place to another and its direction (4.12), as the program turns a task by them. */
	struct FWorldMapHeading
	{
		int16 Heading = 0;
		int32 DirectionX = 0;
		int32 DirectionZ = 0;
	};

	WELTALLCORE_API FWorldMapHeading WorldMapDirectionTo(const FWorldMapVector& From, const FWorldMapVector& To, const FGteArctangents& Arctangents);

	/** A cylinder a moving object stands in for the party's moves: its task slot, place, height and radius. */
	struct FWorldMapCollider
	{
		int16 Slot = 0;
		int32 X = 0;
		int32 Y = 0;
		int32 Z = 0;
		int32 Height = 0;
		int16 Radius = 0;
	};

	/** What a place touches among the colliders: none, near (within 16 units more), touching, and whose. */
	struct FWorldMapContact
	{
		uint8 Contact = 0;
		uint8 Slot = 0;

		bool operator==(const FWorldMapContact&) const = default;
	};

	/** The colliders registered this tick (up to 32; the count wraps past them as the program's does). */
	struct FWorldMapColliders
	{
		static constexpr int32 Capacity = 32;

		std::array<FWorldMapCollider, Capacity> Entries{};
		int16 Count = 0;

		/** A collider added: slot Slot's place, radius and height. */
		WELTALLCORE_API void Add(int32 Slot, const FWorldMapVector& Place, int32 Radius, int32 Height);
		/** The first collider overlapping the place's height that the place touches or nears. */
		WELTALLCORE_API FWorldMapContact Find(const FWorldMapGround& Ground, const FWorldMapVector& Place, int32 Radius, int32 Height) const;
	};
}
