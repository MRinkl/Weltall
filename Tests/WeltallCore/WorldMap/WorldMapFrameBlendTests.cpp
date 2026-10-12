#include "Graphics/GteVectors.h"
#include "WorldMap/WorldMapFrameBlend.h"

#include <catch_amalgamated.hpp>

#include <array>

using namespace Weltall;

// A world map frame drawn between two ticks: each anchor's polygons moved from where the latest tick put them toward
// where the tick before did, the world's about the target each tick drew it about.
namespace
{
	using FPoint = std::array<float, 3>;

	constexpr int32 WorldUnits = 0x8000;

	FWorldMapFrame MakeFrame(uint32 Tick, int16 Yaw, int32 TargetX, int32 TargetZ)
	{
		FWorldMapFrame Frame;
		Frame.Tick = Tick;
		Frame.Yaw = Yaw;
		Frame.Camera = RotationYxz({0x100, static_cast<int16>(-Yaw), 0});
		Frame.Camera.T = {0, 300, 2000};
		Frame.TargetX = TargetX;
		Frame.TargetZ = TargetZ;
		Frame.WorldUnitsX = WorldUnits;
		Frame.WorldUnitsZ = WorldUnits;
		Frame.ProjectionDistance = 256;
		Frame.ScreenCenterY = 140;
		Frame.Anchors.push_back({FWorldMapAnchor::MakeKey(EWorldMapAnchorKind::World, 0), Frame.Camera});
		return Frame;
	}

	FPoint Through(const FGteMatrix& Matrix, const FPoint& Point)
	{
		FPoint Result{};
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			Result[Row] = (Matrix.M[Row][0] * Point[0] + Matrix.M[Row][1] * Point[1] + Matrix.M[Row][2] * Point[2]) / static_cast<float>(FGteMatrix::One)
				+ static_cast<float>(Matrix.T[Row]);
		}
		return Result;
	}

	FPoint Moved(const FWorldMapViewMove& Move, const FPoint& Point)
	{
		FPoint Result{};
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			Result[Row] = Move[Row][0] * Point[0] + Move[Row][1] * Point[1] + Move[Row][2] * Point[2] + Move[Row][3];
		}
		return Result;
	}

	/** A point of the world as a frame draws it about its target (x east, z south). */
	FPoint AboutTarget(const FWorldMapFrame& Frame, float X, float Y, float Z)
	{
		return {X - static_cast<float>(Frame.TargetX), Y, -(Z - static_cast<float>(Frame.TargetZ))};
	}

	void CheckNear(const FPoint& Actual, const FPoint& Expected)
	{
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			CHECK(Actual[Axis] == Catch::Approx(Expected[Axis]).margin(0.05));
		}
	}

	FWorldMapFrameBlend Blend(const FWorldMapFrame& Latest, const FWorldMapFrame& Previous, float Alpha)
	{
		std::vector<int32> Matches;
		std::vector<std::pair<uint32, int32>> Scratch;
		MatchWorldMapAnchors(Previous, Latest, Matches, Scratch);
		FWorldMapFrameBlend Result;
		BlendWorldMapFrames(Latest, &Previous, Matches, Alpha, Result);
		return Result;
	}

	bool IsUnmoved(const FWorldMapViewMove& Move)
	{
		const FWorldMapViewMove Identity{{{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}}};
		return Move == Identity;
	}

	FGteMatrix SpriteAt(int32 X, int32 Y, int32 Z)
	{
		FGteMatrix Matrix = FGteMatrix::Identity();
		Matrix.T = {X, Y, Z};
		return Matrix;
	}
}

TEST_CASE("World map anchors pair by key", "[worldmap][blend]")
{
	FWorldMapFrame Earlier = MakeFrame(1, 0, 1000, 1000);
	FWorldMapFrame Later = MakeFrame(2, 0, 1000, 1000);
	Earlier.Anchors.push_back({FWorldMapAnchor::MakeKey(EWorldMapAnchorKind::Sprite, 4), SpriteAt(0, 0, 1000)});
	Earlier.Anchors.push_back({FWorldMapAnchor::MakeKey(EWorldMapAnchorKind::Sprite, 1), SpriteAt(0, 0, 1000)});
	Later.Anchors.push_back({FWorldMapAnchor::MakeKey(EWorldMapAnchorKind::Sprite, 1), SpriteAt(0, 0, 1000)});
	Later.Anchors.push_back({FWorldMapAnchor::MakeKey(EWorldMapAnchorKind::Particle, 4), SpriteAt(0, 0, 1000)});
	std::vector<int32> Matches;
	std::vector<std::pair<uint32, int32>> Scratch;
	MatchWorldMapAnchors(Earlier, Later, Matches, Scratch);
	CHECK(Matches == std::vector<int32>{0, 2, -1});
}

TEST_CASE("The world map's terrain is drawn between ticks as the camera turns and its target moves", "[worldmap][blend]")
{
	const FWorldMapFrame Previous = MakeFrame(7, 0x20, 1000, 2000);
	const FWorldMapFrame Latest = MakeFrame(8, 0x30, 1012, 1990);
	const FPoint Then = Through(Previous.Camera, AboutTarget(Previous, 1200, -50, 2300));
	const FPoint Now = Through(Latest.Camera, AboutTarget(Latest, 1200, -50, 2300));
	CheckNear(Moved(Blend(Latest, Previous, 0.0f).Moves[0], Now), Then);
	CheckNear(Moved(Blend(Latest, Previous, 1.0f).Moves[0], Now), Now);
	const FPoint Between = Moved(Blend(Latest, Previous, 0.5f).Moves[0], Now);
	CHECK(Between[0] > std::min(Then[0], Now[0]));
	CHECK(Between[0] < std::max(Then[0], Now[0]));
}

TEST_CASE("The world map's terrain blends across the world's wrap", "[worldmap][blend]")
{
	const FWorldMapFrame Previous = MakeFrame(7, 0x20, WorldUnits - 8, 2000);
	const FWorldMapFrame Latest = MakeFrame(8, 0x20, 8, 2000);
	// The point lies 100 units east of the seam; the tick before drew it about the target west of the seam.
	const FPoint Then = Through(Previous.Camera, {108.0f, 0.0f, 0.0f});
	const FPoint Now = Through(Latest.Camera, AboutTarget(Latest, 100, 0, 2000));
	CheckNear(Moved(Blend(Latest, Previous, 0.0f).Moves[0], Now), Then);
}

TEST_CASE("A world map sprite moves between ticks, but not when it jumped or turned about", "[worldmap][blend]")
{
	FWorldMapFrame Previous = MakeFrame(3, 0, 1000, 1000);
	FWorldMapFrame Latest = MakeFrame(4, 0, 1000, 1000);
	const uint32 Key = FWorldMapAnchor::MakeKey(EWorldMapAnchorKind::Sprite, 1);
	Previous.Anchors.push_back({Key, SpriteAt(10, 0, 1500)});
	Latest.Anchors.push_back({Key, SpriteAt(30, 0, 1500)});
	const FPoint Corner = Through(Latest.Anchors[1].Matrix, {5.0f, -20.0f, 0.0f});
	CheckNear(Moved(Blend(Latest, Previous, 0.5f).Moves[1], Corner), {25.0f, -20.0f, 1500.0f});

	Latest.Anchors[1].Matrix = SpriteAt(1000, 0, 1500);
	CHECK(IsUnmoved(Blend(Latest, Previous, 0.5f).Moves[1]));

	Latest.Anchors[1].Matrix = SpriteAt(30, 0, 1500);
	Latest.Anchors[1].Matrix.M[0][0] = -FGteMatrix::One;
	CHECK(IsUnmoved(Blend(Latest, Previous, 0.5f).Moves[1]));
}

TEST_CASE("Nothing of the world map blends across a cut", "[worldmap][blend]")
{
	const FWorldMapFrame Previous = MakeFrame(3, 0, 1000, 1000);
	const auto Unmoved = [&Previous](const FWorldMapFrame& Latest)
	{
		const FWorldMapFrameBlend Result = Blend(Latest, Previous, 0.0f);
		return IsUnmoved(Result.Moves[0]) && Result.ProjectionDistance == static_cast<float>(Latest.ProjectionDistance);
	};
	CHECK_FALSE(Unmoved(MakeFrame(4, 0x10, 1000, 1000)));
	CHECK(Unmoved(MakeFrame(5, 0x10, 1000, 1000)));
	CHECK(Unmoved(MakeFrame(4, 0x10, 3000, 1000)));
	CHECK(Unmoved(MakeFrame(4, 0x200, 1000, 1000)));
}

TEST_CASE("The world map's horizon band scrolls between ticks with the yaw, across its wrap", "[worldmap][blend]")
{
	const FWorldMapFrame Previous = MakeFrame(3, 0xFFA, 1000, 1000);
	const FWorldMapFrame Latest = MakeFrame(4, 6, 1000, 1000);
	CHECK(Blend(Latest, Previous, 0.0f).HorizonScroll == Catch::Approx(-3.0f));
	CHECK(Blend(Latest, Previous, 0.5f).HorizonScroll == Catch::Approx(-1.5f));
	CHECK(Blend(Latest, Previous, 1.0f).HorizonScroll == Catch::Approx(0.0f));
}

TEST_CASE("A world map anchor placed finer than the GTE's numbers blends as it was placed", "[worldmap][blend]")
{
	FWorldMapFrame Previous = MakeFrame(3, 0, 1000, 1000);
	FWorldMapFrame Latest = MakeFrame(4, 0, 1000, 1000);
	const uint32 Key = FWorldMapAnchor::MakeKey(EWorldMapAnchorKind::Gear, 0x1003);
	// A model at a fiftieth of its size and a little more, which a 4.12 matrix rounds: its real numbers go beside it.
	static constexpr float Scale = 0.0203f;
	const auto Placed = [](float X) {
		FWorldMapAnchor Anchor;
		Anchor.Matrix = SpriteAt(static_cast<int32>(X), 0, 1500);
		Anchor.bFine = true;
		Anchor.Fine.M = {{{Scale, 0.0f, 0.0f}, {0.0f, Scale, 0.0f}, {0.0f, 0.0f, Scale}}};
		Anchor.Fine.T = {X, 0.25f, 1500.5f};
		return Anchor;
	};
	Previous.Anchors.push_back(Placed(10.5f));
	Previous.Anchors.back().Key = Key;
	Latest.Anchors.push_back(Placed(30.5f));
	Latest.Anchors.back().Key = Key;
	const FPoint Local{1000.0f, -2000.0f, 500.0f};
	const FPoint Corner{30.5f + Scale * Local[0], 0.25f + Scale * Local[1], 1500.5f + Scale * Local[2]};
	CheckNear(Moved(Blend(Latest, Previous, 0.5f).Moves[1], Corner), {20.5f + Scale * Local[0], 0.25f + Scale * Local[1], 1500.5f + Scale * Local[2]});
	CheckNear(Moved(Blend(Latest, Previous, 0.0f).Moves[1], Corner), {10.5f + Scale * Local[0], 0.25f + Scale * Local[1], 1500.5f + Scale * Local[2]});

	// Fine in one tick only, its matrices are what the two share.
	Previous.Anchors.back().bFine = false;
	const FPoint Whole = Through(Latest.Anchors[1].Matrix, {5.0f, -20.0f, 0.0f});
	CheckNear(Moved(Blend(Latest, Previous, 0.5f).Moves[1], Whole), {25.0f, -20.0f, 1500.0f});
}
