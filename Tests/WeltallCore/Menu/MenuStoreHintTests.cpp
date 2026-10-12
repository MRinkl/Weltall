#include "Menu/MenuStoreHint.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Menu/MenuDrawing.h"
#include "Menu/MenuResources.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>

using namespace Weltall;

// The File screen's hint of the store pairs its two cards do not show.
namespace
{
	constexpr uint16 L1Role = 0x04;
	constexpr uint16 R1Role = 0x08;
	constexpr int32 ScreenWidth = 320;
	/** Where the cards' first row of cells begins. */
	constexpr int32 CardsTop = 14;

	int16 Left(const FMenuPrimitive& Quad)
	{
		return std::min(Quad.Corners[0].X, Quad.Corners[1].X);
	}

	int16 Right(const FMenuPrimitive& Quad)
	{
		return std::max(Quad.Corners[0].X, Quad.Corners[1].X);
	}
}

TEST_CASE("The File screen's hint shows L1 and R1 about an orb a pair of stores, the shown pair's lit", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Resources = FMenuResources::Load(Disc->Image(), *Index, *Executable);
	REQUIRE(Resources);
	const FMenuSpriteSheet& Sheet = Resources->Sheet();

	// Nothing while the cards are not shown.
	std::vector<FMenuPrimitive> Hint;
	AddStorePairHint({false, 0, 8}, Sheet, Hint);
	CHECK(Hint.empty());

	// The third pair of four: each hint its arrow and its box, four orbs between them.
	AddStorePairHint({true, 4, 8}, Sheet, Hint);
	REQUIRE(Hint.size() == 8);
	CHECK(Hint[1].ButtonRole == L1Role);
	CHECK(Hint[7].ButtonRole == R1Role);
	for (std::size_t Orb = 2; Orb < 6; ++Orb)
	{
		INFO(Orb);
		CHECK(Hint[Orb].ButtonRole == 0);
		CHECK(Right(Hint[Orb]) - Left(Hint[Orb]) == 8);
		// The lit orb's texels are another orb's than the rest's.
		CHECK((Hint[Orb].Corners[0].U == Hint[2].Corners[0].U) == (Orb != 4));
	}

	// L1's box, the orbs and R1's box from left to right, clear of one another.
	for (std::size_t Quad = 1; Quad + 2 < Hint.size(); ++Quad)
	{
		CHECK(Right(Hint[Quad]) < Left(Hint[Quad + 1]));
	}
	CHECK(Right(Hint[5]) < Left(Hint[7]));

	// On the screen, over the cards, about the screen's middle.
	int16 Begin = ScreenWidth;
	int16 End = 0;
	for (const FMenuPrimitive& Quad : Hint)
	{
		Begin = std::min(Begin, Left(Quad));
		End = std::max(End, Right(Quad));
		CHECK(Quad.Space == EMenuSpace::Screen);
		CHECK(Quad.Corners[0].Y >= 0);
		CHECK(Quad.Corners[2].Y <= CardsTop);
	}
	CHECK(Begin >= 0);
	CHECK(Begin + End == Catch::Approx(ScreenWidth).margin(1));

	// The display finds both buttons' pictures to show as the player's bindings.
	FMenuFrame Frame;
	Frame.Primitives = Hint;
	FMenuDrawBuilder Builder;
	FDrawList List;
	Builder.Build(Frame, nullptr, 1.0f, List);
	REQUIRE(Builder.Icons.size() == 2);
	CHECK(Builder.Icons[0].Slot == EMenuIconSlot::Hint);
	CHECK(Builder.Icons[1].Role == R1Role);

	// Another pair lights another orb; a single pair needs no hint.
	std::vector<FMenuPrimitive> First;
	AddStorePairHint({true, 0, 8}, Sheet, First);
	REQUIRE(First.size() == 8);
	CHECK(First[2].Corners[0].U == Hint[4].Corners[0].U);
	CHECK(First[4].Corners[0].U == Hint[2].Corners[0].U);
	std::vector<FMenuPrimitive> Single;
	AddStorePairHint({true, 0, 2}, Sheet, Single);
	CHECK(Single.empty());
}
