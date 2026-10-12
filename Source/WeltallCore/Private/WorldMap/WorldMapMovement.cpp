#include "WorldMap/WorldMapMovement.h"

#include "Graphics/Gte.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"
#include "WorldMap/WorldMapTables.h"

#include <cstdlib>

namespace Weltall
{
	namespace
	{
		/** A cell is 128 units: 2^19 in 20.12. */
		constexpr int32 CellShift = 19;
		constexpr uint32 CellMask = 0xFFF80000u;
		constexpr int32 CellSize = 1 << CellShift;
		/** A slide along an axis (4.12). */
		constexpr int32 SideStep = 0x1000;
		/** The party stands 4 units above the ground's surface. */
		constexpr int32 StandingLift = 0x4000;
		/** The Yggdrasil flies at least 32 units above the ground, no lower than 32 below sea level, at most 640 up. */
		constexpr int32 FlyingClearance = 0x20000;
		constexpr int32 FlyingCeiling = -0x280000;
		constexpr int32 ArrivalUnits = 5;

		int32 Multiply(int32 A, int32 B)
		{
			return static_cast<int32>(static_cast<uint32>(A) * static_cast<uint32>(B));
		}

		int32 Step(int32 Direction, int32 Speed)
		{
			return Multiply(Direction, Speed) >> 12;
		}

		/**
		 * One cell boundary of a move, east or any other way: the path's point just
		 * before the boundary, still in the start's cell, then just past it. The first blocked stops
		 * the move there (Stopped); only the second blocked blocks the crossing.
		 */
		EWorldMapCrossing Cross(const FWorldMapGround& Ground, const FWorldMapVector& Position, const FWorldMapVector& Direction, int32 TravelMode,
			bool bAlongX, int32 First, int32 Second, EWorldMapCrossing Blocked, FWorldMapVector& Stop)
		{
			FWorldMapVector Near = Stop;
			FWorldMapVector Far = Stop;
			if (bAlongX)
			{
				const int32 Slope = static_cast<int32>(static_cast<uint32>(Direction.Z) << 12) / Direction.X;
				Near.X = Position.X + First;
				Near.Z = (Multiply(Slope, First) >> 12) + Position.Z;
				Far.X = Position.X + Second;
				Far.Z = (Multiply(Slope, Second) >> 12) + Position.Z;
			}
			else
			{
				const int32 Slope = static_cast<int32>(static_cast<uint32>(Direction.X) << 12) / Direction.Z;
				Near.Z = Position.Z + First;
				Near.X = (Multiply(Slope, First) >> 12) + Position.X;
				Far.Z = Position.Z + Second;
				Far.X = (Multiply(Slope, Second) >> 12) + Position.X;
			}
			Ground.Wrap(Near);
			if (!Ground.CanTraverse(TravelMode, Ground.TerrainType(Near.X, Near.Z)))
			{
				Stop.X = Near.X;
				Stop.Z = Near.Z;
				return EWorldMapCrossing::Stopped;
			}
			Ground.Wrap(Far);
			return Ground.CanTraverse(TravelMode, Ground.TerrainType(Far.X, Far.Z)) ? EWorldMapCrossing::Free : Blocked;
		}

		enum ECrossFlag : int32
		{
			East = 1,
			West = 2,
			South = 4,
			North = 8,
		};
	}

	EWorldMapCrossing CheckWorldMapCellCrossing(const FWorldMapGround& Ground, const FWorldMapVector& Position, const FWorldMapVector& Direction,
		int32 Speed, int32 TravelMode, FWorldMapVector& Stop)
	{
		FWorldMapVector Destination = Position;
		Destination.X = Position.X + Step(Direction.X, Speed);
		Destination.Z = Position.Z + Step(Direction.Z, Speed);
		int32 Flags = 0;
		if (Destination.X >> CellShift < Position.X >> CellShift)
		{
			Flags = West;
		}
		else if (Position.X >> CellShift < Destination.X >> CellShift)
		{
			Flags = East;
		}
		const int16 StartRow = static_cast<int16>(Position.Z >> CellShift);
		const int16 EndRow = static_cast<int16>(Destination.Z >> CellShift);
		if (EndRow < StartRow)
		{
			Flags |= North;
		}
		else if (StartRow < EndRow)
		{
			Flags |= South;
		}

		const auto CrossEast = [&]
		{
			const int32 Offset = static_cast<int32>((static_cast<uint32>(Destination.X) & CellMask) - static_cast<uint32>(Position.X));
			return Cross(Ground, Position, Direction, TravelMode, true, Offset - 1, Offset, EWorldMapCrossing::BlockedAcrossX, Stop);
		};
		const auto CrossWest = [&]
		{
			const int32 Offset = static_cast<int32>((static_cast<uint32>(Position.X) & CellMask) - static_cast<uint32>(Position.X));
			return Cross(Ground, Position, Direction, TravelMode, true, Offset, Offset - 1, EWorldMapCrossing::BlockedAcrossX, Stop);
		};
		const auto CrossSouth = [&]
		{
			const int32 Offset = static_cast<int32>((static_cast<uint32>(Destination.Z) & CellMask) - static_cast<uint32>(Position.Z));
			return Cross(Ground, Position, Direction, TravelMode, false, Offset - 1, Offset, EWorldMapCrossing::BlockedAcrossZ, Stop);
		};
		const auto CrossNorth = [&]
		{
			const int32 Offset = static_cast<int32>((static_cast<uint32>(Position.Z) & CellMask) - static_cast<uint32>(Position.Z));
			return Cross(Ground, Position, Direction, TravelMode, false, Offset, Offset - 1, EWorldMapCrossing::BlockedAcrossZ, Stop);
		};
		// Across a corner, the side of the path the corner lies on (the GTE's NCLIP with the corner as its middle point) picks the boundary tested first.
		const auto CornerSide = [&](bool bEast, bool bSouth)
		{
			const int32 CornerX = static_cast<int32>((static_cast<uint32>(Position.X) & CellMask) + (bEast ? CellSize : 0));
			const int32 CornerZ = static_cast<int32>((static_cast<uint32>(Position.Z) & CellMask) + (bSouth ? CellSize : 0));
			return Nclip(static_cast<int16>((Position.X - CornerX) >> 12), static_cast<int16>((Position.Z - CornerZ) >> 12), 0, 0,
				static_cast<int16>((Destination.X - CornerX) >> 12), static_cast<int16>((Destination.Z - CornerZ) >> 12));
		};
		const auto Either = [](EWorldMapCrossing FirstResult, auto&& Then)
		{
			return FirstResult != EWorldMapCrossing::Free ? FirstResult : Then();
		};

		EWorldMapCrossing Result = EWorldMapCrossing::Free;
		switch (Flags)
		{
		case East:
			Result = CrossEast();
			break;
		case West:
			Result = CrossWest();
			break;
		case South:
			Result = CrossSouth();
			break;
		case North:
			Result = CrossNorth();
			break;
		case East | South:
		{
			const int32 Side = CornerSide(true, true);
			Result = Side < 0 ? Either(CrossEast(), CrossSouth) : Side == 0 ? CrossEast() : Either(CrossSouth(), CrossEast);
			break;
		}
		case West | South:
		{
			const int32 Side = CornerSide(false, true);
			Result = Side > 0 ? Either(CrossWest(), CrossSouth) : Side == 0 ? CrossWest() : Either(CrossSouth(), CrossWest);
			break;
		}
		case East | North:
		{
			const int32 Side = CornerSide(true, false);
			Result = Side > 0 ? Either(CrossEast(), CrossNorth) : Side == 0 ? CrossEast() : Either(CrossNorth(), CrossEast);
			break;
		}
		case West | North:
		{
			const int32 Side = CornerSide(false, false);
			Result = Side < 0 ? Either(CrossWest(), CrossNorth) : Side == 0 ? CrossWest() : Either(CrossNorth(), CrossWest);
			break;
		}
		default:
			break;
		}
		if (Result != EWorldMapCrossing::Free)
		{
			return Result;
		}
		Ground.Wrap(Destination);
		if (!Ground.CanTraverse(TravelMode, Ground.TerrainType(Destination.X, Destination.Z)))
		{
			Stop.X = Destination.X;
			Stop.Z = Destination.Z;
			return EWorldMapCrossing::Stopped;
		}
		return EWorldMapCrossing::Free;
	}

	bool WorldMapTerrainStep(const FWorldMapGround& Ground, const FWorldMapVector& Position, const FWorldMapVector& Direction,
		FWorldMapVector& Result, int32 Speed, int32 TravelMode)
	{
		FWorldMapVector Stop;
		switch (CheckWorldMapCellCrossing(Ground, Position, Direction, Speed, TravelMode, Stop))
		{
		case EWorldMapCrossing::Free:
			return true;
		case EWorldMapCrossing::Stopped:
		{
			const std::array<int32, 2>& Line = Ground.Tables().SlideDirections[Ground.Attribute(Stop.X, Stop.Z) & 0xF];
			const int32 Along = Multiply(Direction.X, Line[0]) + Multiply(Direction.Z, Line[1]);
			if (Along == 0)
			{
				Result.X = 0;
				Result.Y = 0;
				Result.Z = 0;
			}
			else
			{
				Result.X = Along < 0 ? -Line[0] : Line[0];
				Result.Z = Along < 0 ? -Line[1] : Line[1];
			}
			return false;
		}
		case EWorldMapCrossing::BlockedAcrossZ:
			Result.X = Direction.X < 0 ? -SideStep : SideStep;
			Result.Y = 0;
			Result.Z = 0;
			return false;
		case EWorldMapCrossing::BlockedAcrossX:
			Result.X = 0;
			Result.Y = 0;
			Result.Z = Direction.Z < 0 ? -SideStep : SideStep;
			return false;
		}
		return false;
	}

	bool MoveOnWorldMapGround(const FWorldMapGround& Ground, const FWorldMapVector& Position, const FWorldMapVector& Direction,
		FWorldMapVector& Result, int32 Speed, int32 TravelMode)
	{
		Result.X = Position.X + Step(Direction.X, Speed);
		Result.Z = Position.Z + Step(Direction.Z, Speed);
		Ground.Wrap(Result);
		Result.Y = Ground.Height(Result.X, Result.Z) - StandingLift;
		return WorldMapTerrainStep(Ground, Position, Direction, Result, Speed, TravelMode);
	}

	bool MoveInWorldMapAir(const FWorldMapGround& Ground, const FWorldMapVector& Position, const FWorldMapVector& Direction,
		FWorldMapVector& Result, int32 Speed, int32 TravelMode)
	{
		Result.X = Position.X + Step(Direction.X, Speed);
		Result.Y = Position.Y + Step(Direction.Y, Speed);
		Result.Z = Position.Z + Step(Direction.Z, Speed);
		Ground.Wrap(Result);
		int32 Floor = Ground.Height(Result.X, Result.Z) - FlyingClearance;
		if (Floor > FlyingClearance)
		{
			Floor = FlyingClearance;
		}
		int32 Height = Result.Y;
		if (Floor < Height || Floor < FlyingCeiling)
		{
			Height = Floor;
			Result.Y = Floor;
		}
		if (FlyingCeiling < Floor && Height < FlyingCeiling)
		{
			Result.Y = FlyingCeiling;
		}
		return WorldMapTerrainStep(Ground, Position, Direction, Result, Speed, TravelMode);
	}

	int32 WalkTowardWorldMapPoint(const FWorldMapGround& Ground, FWorldMapVector& Position, const FWorldMapVector& Direction, int32 Speed,
		int32 TargetX, int32 TargetZ)
	{
		const int32 HalfSpeed = static_cast<int16>(Speed) / 2;
		int32 Arrived = 0;
		if (std::abs(TargetX - (Position.X >> 12)) < ArrivalUnits)
		{
			Arrived |= 1;
		}
		else
		{
			Position.X += Multiply(Direction.X, HalfSpeed);
		}
		if (std::abs(TargetZ - (Position.Z >> 12)) < ArrivalUnits)
		{
			Arrived |= 2;
		}
		else
		{
			Position.Z += Multiply(Direction.Z, HalfSpeed);
		}
		Ground.Wrap(Position);
		Position.Y = Ground.Height(Position.X, Position.Z);
		return Arrived;
	}

	FWorldMapHeading WorldMapDirectionTo(const FWorldMapVector& From, const FWorldMapVector& To, const FGteArctangents& Arctangents)
	{
		FWorldMapHeading Heading;
		const int32 Angle = (GteRatan2(To.Z - From.Z, To.X - From.X, Arctangents) + 0x400) & 0xFFF;
		Heading.Heading = static_cast<int16>(Angle);
		Heading.DirectionX = GteSin(Angle);
		Heading.DirectionZ = -GteCos(Heading.Heading);
		return Heading;
	}

	void FWorldMapColliders::Add(int32 Slot, const FWorldMapVector& Place, int32 Radius, int32 Height)
	{
		FWorldMapCollider& Entry = Entries[static_cast<std::size_t>(Count)];
		Entry.Slot = static_cast<int16>(Slot);
		Entry.X = Place.X;
		Entry.Y = Place.Y;
		Entry.Z = Place.Z;
		Entry.Height = static_cast<int16>(Height);
		Entry.Radius = static_cast<int16>(Radius);
		Count = static_cast<int16>((Count + 1) & (Capacity - 1));
	}

	FWorldMapContact FWorldMapColliders::Find(const FWorldMapGround& Ground, const FWorldMapVector& Place, int32 Radius, int32 Height) const
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FWorldMapCollider& Entry = Entries[static_cast<std::size_t>(Index)];
			int32 Lower = 0;
			int32 Upper = 0;
			if (Place.Y < Entry.Y)
			{
				Upper = Place.Y - Height * 0x1000;
				Lower = Entry.Y;
			}
			else
			{
				Upper = Entry.Y - Entry.Height * 0x1000;
				Lower = Place.Y;
			}
			if ((Lower - Upper) >> 12 >= Height + Entry.Height)
			{
				continue;
			}
			int32 DeltaX = (Entry.X - Place.X) >> 12;
			int32 DeltaZ = (Entry.Z - Place.Z) >> 12;
			Ground.WrapDeltaUnits(DeltaX, DeltaZ);
			const int32 Distance = SquareRoot0(Multiply(DeltaX, DeltaX) + Multiply(DeltaZ, DeltaZ));
			if (Distance < Entry.Radius + Radius)
			{
				return {2, static_cast<uint8>(Entry.Slot)};
			}
			if (Distance < Entry.Radius + Radius + 16)
			{
				return {1, static_cast<uint8>(Entry.Slot)};
			}
		}
		return {};
	}
}
