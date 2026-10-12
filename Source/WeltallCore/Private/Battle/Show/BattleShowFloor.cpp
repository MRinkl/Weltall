#include "Battle/Show/BattleShowFloor.h"

#include "Graphics/GteVectors.h"

namespace Weltall
{
	namespace
	{
		FGteLongVector Flat(const FBattleFloorVertex& From, int32 ToX, int32 ToZ)
		{
			return {ToX - From.X, 0, ToZ - From.Z};
		}
	}

	void FBattleFloor::Reset(const std::vector<FBattleFloorVertex>& Vertices, const std::vector<FBattleFloorTriangle>& Triangles)
	{
		Corners = Vertices;
		Faces = Triangles;
		TriedMarks.assign(Faces.size(), 0);
		Search = 1;
		if (Faces.empty())
		{
			Corners.clear();
		}
	}

	bool FBattleFloor::Inside(int32 Triangle, const FGteVector& Point) const
	{
		const FBattleFloorTriangle& Face = Faces[static_cast<std::size_t>(Triangle)];
		for (std::size_t Edge = 0; Edge < 3; ++Edge)
		{
			const FBattleFloorVertex& From = Corners[static_cast<std::size_t>(Face.Corners[Edge])];
			const FBattleFloorVertex& To = Corners[static_cast<std::size_t>(Face.Corners[(Edge + 1) % 3])];
			// The y of (edge x the point's offset), both flat: which side of the edge the point lies.
			if (OuterProduct0(Flat(From, To.X, To.Z), Flat(From, Point[0], Point[2]))[1] < 0)
			{
				return false;
			}
		}
		return true;
	}

	int32 FBattleFloor::SearchNeighbours(const FGteVector& Point, int32 Triangle, int32 Depth)
	{
		if (Triangle < 0)
		{
			return -1;
		}
		uint8& Mark = TriedMarks[static_cast<std::size_t>(Triangle)];
		if (Mark != Search)
		{
			Mark = Search;
			if (Inside(Triangle, Point))
			{
				return Triangle;
			}
		}
		if (Depth > 0)
		{
			for (const int16 Neighbour : Faces[static_cast<std::size_t>(Triangle)].Neighbours)
			{
				const int32 Found = SearchNeighbours(Point, Neighbour, Depth - 1);
				if (Found >= 0)
				{
					return Found;
				}
			}
		}
		return -1;
	}

	std::optional<int32> FBattleFloor::FindTriangle(const FGteVector& Point, int32 Known, int32 Depth)
	{
		if (!HasFloor() || Known >= TriangleCount())
		{
			return -1;
		}
		std::optional<int32> Found;
		for (int32 Attempt = 0; Attempt < Depth; ++Attempt)
		{
			Found = SearchNeighbours(Point, Known, Depth);
			if (*Found >= 0)
			{
				break;
			}
		}
		if (static_cast<uint8>(Search + 1) != 0)
		{
			++Search;
			return Found;
		}
		Search = 1;
		TriedMarks.assign(TriedMarks.size(), 0);
		return Found;
	}

	int32 FBattleFloor::ScanTriangles(const FGteVector& Point) const
	{
		if (!HasFloor())
		{
			return -1;
		}
		for (int32 Triangle = 0; Triangle < TriangleCount(); ++Triangle)
		{
			if (Inside(Triangle, Point))
			{
				return Triangle;
			}
		}
		return -1;
	}

	int32 FBattleFloor::Height(FGteVector& Point, int32 Triangle) const
	{
		if (!HasFloor() || Triangle < 0)
		{
			return -1;
		}
		const FBattleFloorTriangle& Face = Faces[static_cast<std::size_t>(Triangle)];
		const FBattleFloorVertex& First = Corners[static_cast<std::size_t>(Face.Corners[0])];
		const FBattleFloorVertex& Second = Corners[static_cast<std::size_t>(Face.Corners[1])];
		const FBattleFloorVertex& Third = Corners[static_cast<std::size_t>(Face.Corners[2])];
		const FGteLongVector Along = VectorNormal({Second.X - First.X, Second.Y - First.Y, Second.Z - First.Z});
		const FGteLongVector Across = VectorNormal({Third.X - First.X, Third.Y - First.Y, Third.Z - First.Z});
		const FGteLongVector Normal = OuterProduct12(Along, Across);
		if (Normal[1] == 0)
		{
			Point[1] = 0;
		}
		else
		{
			const int32 Rise = -((Point[2] - First.Z) * Normal[2]) - (Point[0] - First.X) * Normal[0];
			Point[1] = static_cast<int16>(First.Y + static_cast<int16>(Rise / Normal[1]));
		}
		return Face.Attribute;
	}

	int32 FBattleFloor::TriangleUnder(const FGteVector& Point, int32 Known)
	{
		const std::optional<int32> Found = FindTriangle(Point, Known, 4);
		return *Found >= 0 ? *Found : ScanTriangles(Point);
	}
}
