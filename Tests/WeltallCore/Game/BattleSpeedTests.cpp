#include "Game/BattleSpeed.h"
#include "Game/BattleSpeedMark.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Text/MesFont.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	constexpr FPadState L1{FPadState::Bit(EPadButton::L1)};
	constexpr FPadState R1{FPadState::Bit(EPadButton::R1)};

	/** Twice a triangle's area. */
	float Doubled(const FDrawPrimitive& Triangle)
	{
		const FDrawVertex& A = Triangle.Corners[0];
		const FDrawVertex& B = Triangle.Corners[1];
		const FDrawVertex& C = Triangle.Corners[2];
		return (B.X - A.X) * (C.Y - A.Y) - (C.X - A.X) * (B.Y - A.Y);
	}

	/** Whether a point lies inside a triangle or on its edge. */
	bool Holds(const FDrawPrimitive& Triangle, const FDrawVertex& Point)
	{
		const auto Side = [&Point](const FDrawVertex& From, const FDrawVertex& To)
		{
			return (To.X - From.X) * (Point.Y - From.Y) - (To.Y - From.Y) * (Point.X - From.X);
		};
		const float First = Side(Triangle.Corners[0], Triangle.Corners[1]);
		const float Second = Side(Triangle.Corners[1], Triangle.Corners[2]);
		const float Third = Side(Triangle.Corners[2], Triangle.Corners[0]);
		return (First <= 0 && Second <= 0 && Third <= 0) || (First >= 0 && Second >= 0 && Third >= 0);
	}
}

TEST_CASE("A battle is fought at the game's pace, twice it or four times it", "[Game]")
{
	CHECK(BattleSpeedRate(EBattleSpeed::Normal) == 1);
	CHECK(BattleSpeedRate(EBattleSpeed::Double) == 2);
	CHECK(BattleSpeedRate(EBattleSpeed::Quadruple) == 4);
	CHECK(BattleSpeedName(EBattleSpeed::Normal).empty());
	CHECK(BattleSpeedName(EBattleSpeed::Double) == "2x");
	CHECK(BattleSpeedName(EBattleSpeed::Quadruple) == "4x");
}

TEST_CASE("R1 steps the battle speed up and L1 back toward the game's pace, no further than either end", "[Game]")
{
	EBattleSpeed Speed = EBattleSpeed::Normal;
	Speed = StepBattleSpeed(Speed, L1);
	CHECK(Speed == EBattleSpeed::Normal);
	Speed = StepBattleSpeed(Speed, R1);
	CHECK(Speed == EBattleSpeed::Double);
	Speed = StepBattleSpeed(Speed, R1);
	CHECK(Speed == EBattleSpeed::Quadruple);
	Speed = StepBattleSpeed(Speed, R1);
	CHECK(Speed == EBattleSpeed::Quadruple);
	Speed = StepBattleSpeed(Speed, L1);
	CHECK(Speed == EBattleSpeed::Double);
	// No other button steps it, and the two together leave it.
	CHECK(StepBattleSpeed(Speed, {}) == EBattleSpeed::Double);
	CHECK(StepBattleSpeed(Speed, FPadState{static_cast<uint16>(0xFFFF & ~(L1.Buttons | R1.Buttons))}) == EBattleSpeed::Double);
	CHECK(StepBattleSpeed(Speed, FPadState{static_cast<uint16>(L1.Buttons | R1.Buttons)}) == EBattleSpeed::Double);
	Speed = StepBattleSpeed(Speed, L1);
	CHECK(Speed == EBattleSpeed::Normal);
}

TEST_CASE("The battle speed's mark is two arrowheads over the speed in the game's font, ending at its screen's right edge", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	FBattleSpeedMarkDrawing Drawing;
	FDrawList List;
	List.Primitives.resize(3);
	const uint32 Blank = Drawing.Pictures().Revision();
	Drawing.Build(EBattleSpeed::Normal, Resident.Font, List);
	CHECK(List.Primitives.empty());
	CHECK(Drawing.Pictures().Revision() == Blank);

	Drawing.Build(EBattleSpeed::Double, Resident.Font, List);
	REQUIRE(List.Primitives.size() == 5);
	const float Right = static_cast<float>(BattleSpeedMarkScreen.Width);
	// The arrowheads: the dark ones first, a light one inside each, the second's point at the right edge.
	for (std::size_t Arrow = 0; Arrow < 4; ++Arrow)
	{
		const FDrawPrimitive& Triangle = List.Primitives[Arrow];
		CHECK(Triangle.CornerCount == 3);
		CHECK_FALSE(Triangle.Texture.has_value());
		CHECK(Triangle.Blend == EDrawBlend::Opaque);
		CHECK(Triangle.Corners[0].X == Triangle.Corners[1].X);
		CHECK(Triangle.Corners[0].X >= 0.0f);
		CHECK(Triangle.Corners[2].X > Triangle.Corners[0].X);
		CHECK(Triangle.Corners[2].X <= Right);
		CHECK(Triangle.Corners[2].Y == Catch::Approx((Triangle.Corners[0].Y + Triangle.Corners[1].Y) / 2));
	}
	for (std::size_t Arrow = 0; Arrow < 2; ++Arrow)
	{
		const FDrawPrimitive& Dark = List.Primitives[Arrow];
		const FDrawPrimitive& Light = List.Primitives[Arrow + 2];
		CHECK(Dark.Corners[0].Color.R < 64);
		CHECK(Light.Corners[0].Color.R > 224);
		CHECK(Doubled(Light) / Doubled(Dark) < 0.6f);
		for (std::size_t Corner = 0; Corner < 3; ++Corner)
		{
			CHECK(Holds(Dark, Light.Corners[Corner]));
		}
	}
	CHECK(List.Primitives[1].Corners[2].X == Right);
	const float ArrowsBottom = List.Primitives[0].Corners[1].Y;

	// The text: "2x" drawn as it is from its picture, under the arrows, its last column at the right edge.
	const FDrawPrimitive& Text = List.Primitives[4];
	REQUIRE(Text.Texture);
	CHECK(Text.Blend == EDrawBlend::Opaque);
	CHECK(Text.Corners[0].Y >= ArrowsBottom);
	CHECK(Text.Corners[1].X == Right);
	CHECK(Text.Corners[2].Y == static_cast<float>(BattleSpeedMarkScreen.Height));
	const int32 Width = static_cast<int32>(Text.Corners[1].X - Text.Corners[0].X);
	CHECK(Width >= 12);
	CHECK(Width <= 20);
	const auto Count = [&Drawing, Width](uint16& Others)
	{
		int32 White = 0;
		Others = 0;
		bool bLastColumn = false;
		for (int32 Row = 0; Row < FMesFont::CellHeight; ++Row)
		{
			for (int32 Column = 0; Column < Width; ++Column)
			{
				const uint16 Unit = Drawing.Pictures().At(Column, Row);
				White += Unit == 0x7FFF ? 1 : 0;
				Others += Unit != 0x7FFF && Unit != 0 ? 1 : 0;
				bLastColumn = bLastColumn || (Column == Width - 1 && Unit != 0);
			}
			// Nothing past the width the sprite draws.
			CHECK(Drawing.Pictures().At(Width, Row) == 0);
		}
		CHECK(bLastColumn);
		return White;
	};
	uint16 Outline = 0;
	const int32 White = Count(Outline);
	CHECK(White > 20);
	CHECK(Outline > White);

	// The same speed again draws no text; another speed's replaces it.
	const uint32 Uploads = Drawing.Pictures().Revision();
	Drawing.Build(EBattleSpeed::Double, Resident.Font, List);
	CHECK(Drawing.Pictures().Revision() == Uploads);
	std::vector<uint16> Two;
	for (int32 Row = 0; Row < FMesFont::CellHeight; ++Row)
	{
		for (int32 Column = 0; Column < 32; ++Column)
		{
			Two.push_back(Drawing.Pictures().At(Column, Row));
		}
	}
	Drawing.Build(EBattleSpeed::Quadruple, Resident.Font, List);
	REQUIRE(List.Primitives.size() == 5);
	CHECK(Drawing.Pictures().Revision() > Uploads);
	std::vector<uint16> Four;
	for (int32 Row = 0; Row < FMesFont::CellHeight; ++Row)
	{
		for (int32 Column = 0; Column < 32; ++Column)
		{
			Four.push_back(Drawing.Pictures().At(Column, Row));
		}
	}
	CHECK(Two != Four);
}

TEST_CASE("The battle speed's mark sits in the picture's top right corner, as large against its height whatever its shape", "[Game]")
{
	// On the game's own screen: its lines are the mark's, five of them from the edges.
	CHECK(PlaceBattleSpeedMark(320, 224) == FDisplayRect{320 - 5 - 24, 5, 24, 24});
	// A 16:9 display of 1080 lines and a 32:9 one: the same mark, from each one's right edge; at 4K twice the size.
	CHECK(PlaceBattleSpeedMark(1920, 1080) == FDisplayRect{1920 - 24 - 116, 24, 116, 116});
	CHECK(PlaceBattleSpeedMark(3840, 1080) == FDisplayRect{3840 - 24 - 116, 24, 116, 116});
	CHECK(PlaceBattleSpeedMark(3840, 2160) == FDisplayRect{3840 - 48 - 231, 48, 231, 231});
}
