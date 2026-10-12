#include "Gears/GearCloth.h"
#include "Graphics/GteVectors.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <vector>

using namespace Weltall;

namespace
{
	FGte ClothGte()
	{
		FGte Gte;
		Gte.Transform = RotationXyz({0, 0, 0});
		Gte.OffsetX = 160 << 16;
		Gte.OffsetY = 120 << 16;
		Gte.ProjectionDistance = 0x200;
		Gte.AverageScale3 = 0x155;
		return Gte;
	}

	/** Three triangles of a strip before the eye, the middle one reaching behind it. */
	FGearCloth StripThroughTheEye()
	{
		FGearCloth Cloth;
		for (const FGteVector& Position : std::vector<FGteVector>{{0, 0, 1000}, {100, 0, 1000}, {0, 100, -200}, {100, 100, 1000}, {0, 200, 1000}})
		{
			FGearClothPoint& Point = Cloth.Points.emplace_back();
			Point.Position = Position;
		}
		Cloth.Triangles = {{{0, 1, 3}, {}}, {{1, 2, 3}, {}}, {{0, 3, 4}, {}}};
		return Cloth;
	}

	std::vector<uint16> DrawnTriangles(EGearClothCulling Culling)
	{
		std::vector<FGearDrawnClothTriangle> Drawn;
		DrawGearCloth(StripThroughTheEye(), ClothGte(), Culling, 0xFFF, 2, 0, 0, Drawn);
		std::vector<uint16> Numbers;
		for (const FGearDrawnClothTriangle& Triangle : Drawn)
		{
			Numbers.push_back(Triangle.Triangle);
		}
		return Numbers;
	}
}

TEST_CASE("A cloth under the original's culling is drawn up to its first triangle a depth leaves out", "[Gears]")
{
	CHECK(DrawnTriangles(EGearClothCulling::Original) == std::vector<uint16>{0});
}

TEST_CASE("A cloth in range is drawn whole, the triangle behind the eye included", "[Gears]")
{
	CHECK(DrawnTriangles(EGearClothCulling::InRange) == std::vector<uint16>{0, 1, 2});
}

TEST_CASE("Corners wind clockwise from the eye as NCLIP finds them on the screen", "[Gears]")
{
	const FGte Gte = ClothGte();
	const std::vector<std::array<FGteVector, 3>> Triangles{
		{{{0, 0, 1000}, {100, 0, 1000}, {0, 100, 1000}}},
		{{{0, 0, 1000}, {0, 100, 1000}, {100, 0, 1000}}},
		{{{-300, 50, 600}, {20, -80, 2400}, {150, 200, 900}}},
		{{{-300, 50, 600}, {150, 200, 900}, {20, -80, 2400}}},
	};
	for (const std::array<FGteVector, 3>& Corners : Triangles)
	{
		const FGteProjection A = Gte.Rtps(Corners[0]);
		const FGteProjection B = Gte.Rtps(Corners[1]);
		const FGteProjection C = Gte.Rtps(Corners[2]);
		CHECK(WindsClockwiseFromEye(A, B, C) == (Nclip(A.ScreenX, A.ScreenY, B.ScreenX, B.ScreenY, C.ScreenX, C.ScreenY) > 0));
	}
}
