#include "Field/FieldRenderState.h"
#include "Field/FieldScreenInVram.h"
#include "Field/FieldVramChanges.h"

#include <catch_amalgamated.hpp>

#include <algorithm>

using namespace Weltall;

namespace
{
	/** The units the regions cover, counted, and whether a unit's screen pixel is where the region says. */
	int32 CoveredUnits(const std::vector<FFieldScreenInVram>& Regions)
	{
		int32 Units = 0;
		for (const FFieldScreenInVram& Region : Regions)
		{
			Units += Region.Rect.Width * Region.Rect.Height;
		}
		return Units;
	}

	const FFieldScreenInVram* RegionAt(const std::vector<FFieldScreenInVram>& Regions, int32 X, int32 Y)
	{
		const auto Found = std::find_if(Regions.begin(), Regions.end(), [X, Y](const FFieldScreenInVram& Region)
		{
			return X >= Region.Rect.X && X < Region.Rect.X + Region.Rect.Width && Y >= Region.Rect.Y && Y < Region.Rect.Y + Region.Rect.Height;
		});
		return Found != Regions.end() ? &*Found : nullptr;
	}

	FFieldVramOperation MovedPicture()
	{
		FFieldVramOperation Move;
		Move.Kind = EFieldVramChange::Copy;
		Move.Rect = {0x2C0, 0x100, 0x140, 0xFF};
		Move.ToX = 0x140;
		Move.ToY = 0xFF;
		return Move;
	}
}

TEST_CASE("A copy of the picture's area while it holds the screen carries the screen's pixels, the rows below it not", "[Field]")
{
	FFieldCover Cover;
	Cover.Taken = 3;
	std::vector<FFieldScreenInVram> Carried;
	CarryScreenInVram({}, MovedPicture(), Cover, Carried);
	REQUIRE(Carried.size() == 1);
	const FFieldScreenInVram& Region = Carried[0];
	CHECK((Region.Rect.X == 0x140 && Region.Rect.Y == 0xFF && Region.Rect.Width == 0x140 && Region.Rect.Height == 0xE0));
	CHECK((Region.ScreenX == 0 && Region.ScreenY == 0));
	CHECK(Region.Brightness == 128);
	CHECK(Region.Taken == 3);

	// Once the area holds what the scripts put there, it is VRAM's.
	Cover.bScreen = false;
	Carried.clear();
	CarryScreenInVram({}, MovedPicture(), Cover, Carried);
	CHECK(Carried.empty());
}

TEST_CASE("A copy of part of the screen's pixels carries that part at its brightness", "[Field]")
{
	FFieldCover Cover;
	Cover.bScreen = false;
	const std::vector<FFieldScreenInVram> Regions = {{{0x200, 0, 0x140, 0xE0}, 0, 0, 128, 1}};
	FFieldVramOperation Copy;
	Copy.Kind = EFieldVramChange::Copy;
	Copy.Rect = {0x1F0, 0x10, 0x20, 0x08};
	Copy.ToX = 0x40;
	Copy.ToY = 0x180;
	Copy.Brightness = 64;
	std::vector<FFieldScreenInVram> Carried;
	CarryScreenInVram(Regions, Copy, Cover, Carried);
	REQUIRE(Carried.size() == 1);
	// Only the half inside the region, which starts 16 units into the copy.
	CHECK((Carried[0].Rect.X == 0x50 && Carried[0].Rect.Y == 0x180 && Carried[0].Rect.Width == 0x10 && Carried[0].Rect.Height == 0x08));
	CHECK((Carried[0].ScreenX == 0 && Carried[0].ScreenY == 0x10));
	CHECK(Carried[0].Brightness == 64);
}

TEST_CASE("An upload over the screen's pixels leaves the rest of them where they were", "[Field]")
{
	std::vector<FFieldScreenInVram> Regions = {{{0x140, 0xFF, 0x140, 0xE0}, 0, 0, 128, 2}};
	WriteOverScreenInVram(Regions, {0x180, 0x120, 0x10, 0x10});
	CHECK(CoveredUnits(Regions) == 0x140 * 0xE0 - 0x100);
	CHECK(RegionAt(Regions, 0x185, 0x125) == nullptr);
	const FFieldScreenInVram* Right = RegionAt(Regions, 0x190, 0x125);
	REQUIRE(Right != nullptr);
	// The unit's screen pixel stays (0x50, 0x26).
	CHECK(Right->ScreenX + (0x190 - Right->Rect.X) == 0x50);
	CHECK(Right->ScreenY + (0x125 - Right->Rect.Y) == 0x26);
	CHECK(Right->Taken == 2);

	// An upload that crosses VRAM's right edge writes over the left one too.
	std::vector<FFieldScreenInVram> Edge = {{{0, 0, 8, 8}, 0, 0, 128, 1}};
	WriteOverScreenInVram(Edge, {0x3FC, 0, 8, 4});
	CHECK(CoveredUnits(Edge) == 64 - 16);
	CHECK(RegionAt(Edge, 2, 2) == nullptr);
	CHECK(RegionAt(Edge, 5, 2) != nullptr);
	CHECK(RegionAt(Edge, 2, 5) != nullptr);
}
