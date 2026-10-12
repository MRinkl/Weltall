#include "Field/FieldPlayerCamera.h"
#include "WorldMap/WorldMapFrameBlend.h"
#include "WorldMap/WorldMapPlayerCamera.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <cmath>

using namespace Weltall;

// The player's view of the world map: offsets laid over the game's camera on its orbit, built as the game builds its
// own, and moved on between ticks by a display.
namespace
{
	using FPoint = std::array<float, 3>;

	/** The camera at rest behind a party on level ground: preset 1's distance and pitch, looking north-west. */
	constexpr FWorldMapCameraBasis AtRest{-40, 880 * 0x1000, -455, 0xC00, 0};

	FPoint Through(const FAffine& Affine, const FPoint& Point)
	{
		const FPoint Turned = Affine.Turn(Point);
		return {Turned[0] + Affine.T[0], Turned[1] + Affine.T[1], Turned[2] + Affine.T[2]};
	}

	FPoint Moved(const FWorldMapViewMove& Move, const FPoint& Point)
	{
		return MoveViewPoint(Move, Point);
	}

	float Apart(const FPoint& A, const FPoint& B)
	{
		return std::sqrt((A[0] - B[0]) * (A[0] - B[0]) + (A[1] - B[1]) * (A[1] - B[1]) + (A[2] - B[2]) * (A[2] - B[2]));
	}

	FWorldMapFrame FrameUnder(uint32 Tick, const FWorldMapCameraOffsets& Offsets)
	{
		const FWorldMapPlayerView View = WorldMapPlayerView(AtRest, Offsets);
		FWorldMapFrame Frame;
		Frame.Tick = Tick;
		Frame.Camera = View.Camera;
		Frame.Yaw = View.Yaw;
		Frame.WorldUnitsX = 0x8000;
		Frame.WorldUnitsZ = 0x8000;
		Frame.ProjectionDistance = 256;
		Frame.ScreenCenterY = 140;
		Frame.bPlayerView = true;
		Frame.CameraBasis = AtRest;
		Frame.ShownOffsets = View.Shown;
		Frame.Anchors.push_back({FWorldMapAnchor::MakeKey(EWorldMapAnchorKind::World, 0), View.Camera});
		Frame.Anchors.push_back({FWorldMapAnchor::MakeKey(EWorldMapAnchorKind::Sky, 0), View.Camera});
		return Frame;
	}
}

TEST_CASE("The world map's view without offsets is the game's camera, bit for bit", "[WorldMap]")
{
	const FWorldMapPlayerView View = WorldMapPlayerView(AtRest, {});
	const std::array<int16, 3> Eye = WorldMapOrbitEye(AtRest.TargetY, AtRest.Distance, AtRest.Pitch, AtRest.Yaw);
	CHECK(View.Eye == Eye);
	CHECK(View.Camera == WorldMapCameraMatrix(Eye, AtRest.Pitch, AtRest.Yaw, AtRest.Roll));
	CHECK(View.Pitch == AtRest.Pitch);
	CHECK(View.Yaw == AtRest.Yaw);
	CHECK(View.EyeBack == 0);
	CHECK(View.Shown == FWorldMapCameraOffsets{});
	CHECK(WorldMapViewTurn({}) == 0);
}

TEST_CASE("The world map's view turns as R1 turns the camera, tilts it steeper and pulls the eye back", "[WorldMap]")
{
	// 45 degrees is one press of R1: the camera's yaw 200h on.
	FWorldMapCameraBasis Turned = AtRest;
	Turned.Yaw = static_cast<int16>(AtRest.Yaw + 0x200);
	const FWorldMapPlayerView ByOffset = WorldMapPlayerView(AtRest, {1.0f, 0.0f, 45.0f});
	const FWorldMapPlayerView ByCamera = WorldMapPlayerView(Turned, {});
	CHECK(ByOffset.Camera == ByCamera.Camera);
	CHECK(ByOffset.Eye == ByCamera.Eye);
	CHECK(ByOffset.Yaw == Turned.Yaw);
	CHECK(ByOffset.Shown.Turn == 45.0f);
	CHECK(WorldMapViewTurn({1.0f, 0.0f, 45.0f}) == 0x200);
	CHECK(WorldMapViewTurn({1.0f, 0.0f, -90.0f}) == -0x400);

	// Twice as far: the eye at the doubled distance, and the drawing's depths that much farther.
	FWorldMapCameraBasis Far = AtRest;
	Far.Distance = AtRest.Distance * 2;
	const FWorldMapPlayerView Zoomed = WorldMapPlayerView(AtRest, {2.0f, 0.0f, 0.0f});
	CHECK(Zoomed.Camera == WorldMapPlayerView(Far, {}).Camera);
	CHECK(Zoomed.EyeBack == 880);
	CHECK(Zoomed.Shown.Zoom == 2.0f);
	CHECK(WorldMapPlayerView(AtRest, {0.5f, 0.0f, 0.0f}).EyeBack == -440);

	// The tilt looks down from higher: the pitch steeper by its degrees, to 85 at most.
	const FWorldMapPlayerView Tilted = WorldMapPlayerView(AtRest, {1.0f, 18.0f, 0.0f});
	CHECK(Tilted.Pitch == AtRest.Pitch - 205);
	CHECK(Tilted.Shown.Tilt == Catch::Approx(18.0f).margin(0.05));
	// Up is the lesser y.
	CHECK(Tilted.Eye[1] < WorldMapPlayerView(AtRest, {}).Eye[1]);
	FWorldMapCameraBasis Steep = AtRest;
	Steep.Pitch = -900;
	CHECK(WorldMapPlayerView(Steep, {1.0f, 20.0f, 0.0f}).Pitch == -967);
	Steep.Pitch = -1000;
	CHECK(WorldMapPlayerView(Steep, {1.0f, 20.0f, 0.0f}).Pitch == -1000);
	CHECK(WorldMapPlayerView(AtRest, {1.0f, -10.0f, 0.0f}).Pitch == AtRest.Pitch + 114);
}

TEST_CASE("The world map's view without the game's whole steps follows the game's own camera", "[WorldMap]")
{
	const std::array<FWorldMapCameraOffsets, 4> Cases{{{}, {1.5f, 10.0f, 30.0f}, {0.5f, -10.0f, -170.0f}, {2.0f, 20.0f, 90.0f}}};
	const std::array<FPoint, 3> Points{{{0.0f, -40.0f, 0.0f}, {300.0f, -120.0f, -500.0f}, {-800.0f, 0.0f, 250.0f}}};
	for (const FWorldMapCameraOffsets& Offsets : Cases)
	{
		const FAffine Whole = FAffine::From(WorldMapPlayerView(AtRest, Offsets).Camera);
		const FAffine Smooth = WorldMapPlayerViewAffine(AtRest, Offsets);
		for (const FPoint& Point : Points)
		{
			// The game's matrices round to 1/4096 and its eye to whole units.
			CHECK(Apart(Through(Whole, Point), Through(Smooth, Point)) < 3.0f);
		}
	}
	// The target stands straight ahead of the eye, at the distance.
	const FPoint Target = Through(WorldMapPlayerViewAffine(AtRest, {1.5f, 10.0f, 30.0f}), {0.0f, static_cast<float>(AtRest.TargetY), 0.0f});
	CHECK(std::fabs(Target[0]) < 0.01f);
	CHECK(std::fabs(Target[1]) < 0.01f);
	CHECK(Target[2] == Catch::Approx(880.0f * 1.5f).margin(0.01));
}

TEST_CASE("The world map's offsets take the view's input within their ranges", "[WorldMap]")
{
	FFieldViewInput Input;
	Input.Turn = 30.0f;
	Input.Tilt = 5.0f;
	Input.Zoom = -1.0f;
	FWorldMapCameraOffsets Offsets = MoveWorldMapCameraOffsets({}, Input);
	CHECK(Offsets.Turn == 30.0f);
	CHECK(Offsets.Tilt == 5.0f);
	CHECK(Offsets.Zoom == 2.0f);
	// Past the ranges the zoom and the tilt stop, and the turn goes on round.
	for (int32 Step = 0; Step < 10; ++Step)
	{
		Offsets = MoveWorldMapCameraOffsets(Offsets, Input);
	}
	CHECK(Offsets.Zoom == FWorldMapCameraOffsets::MaxZoom);
	CHECK(Offsets.Tilt == FWorldMapCameraOffsets::MaxTilt);
	CHECK(Offsets.Turn == Catch::Approx(-30.0f).margin(0.001));
	Input.Zoom = 4.0f;
	Input.Tilt = -100.0f;
	Offsets = MoveWorldMapCameraOffsets(Offsets, Input);
	CHECK(Offsets.Zoom == FWorldMapCameraOffsets::MinZoom);
	CHECK(Offsets.Tilt == FWorldMapCameraOffsets::MinTilt);
	FFieldViewInput Reset;
	Reset.bReset = true;
	Reset.Turn = 50.0f;
	CHECK(MoveWorldMapCameraOffsets(Offsets, Reset) == FWorldMapCameraOffsets{});
}

TEST_CASE("A display draws the world map under its frames' camera unrounded, the sky under its own", "[WorldMap]")
{
	const FWorldMapFrame Latest = FrameUnder(8, {});
	const FPoint InWorld{120.0f, -60.0f, -340.0f};
	const FPoint Drawn = Through(FAffine::From(Latest.Camera), InWorld);
	const FPoint Unrounded = Through(WorldMapPlayerViewAffine(AtRest, {}), InWorld);
	REQUIRE(Apart(Drawn, Unrounded) > 0.05f);
	FWorldMapFrameBlend Blend;
	BlendWorldMapFrames(Latest, nullptr, {}, 1.0f, Blend);
	UnroundWorldMapBlend(Latest, nullptr, 1.0f, Blend);
	CHECK(Apart(Moved(Blend.Moves[0], Drawn), Unrounded) < 0.01f);
	CHECK(Blend.Moves[1] == UnmovedView);

	// Halfway between two ticks, halfway between the two frames' own unrounding.
	const FWorldMapFrame Previous = FrameUnder(7, {1.0f, 0.0f, 10.0f});
	const FWorldMapFrame Turned = FrameUnder(8, {1.0f, 0.0f, 16.0f});
	BlendWorldMapFrames(Turned, &Previous, std::vector<int32>{0, 1}, 0.5f, Blend);
	REQUIRE(Blend.bBlended);
	const FWorldMapViewMove Between = Blend.Moves[0];
	UnroundWorldMapBlend(Turned, &Previous, 0.5f, Blend);
	FAffine Back;
	REQUIRE(FAffine::From(Turned.Camera).Invert(Back));
	const FPoint DrawnTurned = Through(FAffine::From(Turned.Camera), InWorld);
	const FPoint Halfway = Moved(Between, DrawnTurned);
	CHECK(Apart(Moved(Blend.Moves[0], DrawnTurned), Halfway) < 0.6f);
	CHECK(Apart(Moved(Blend.Moves[0], DrawnTurned), Halfway) > 0.0f);

	// A cutscene's camera, or a script's, is the game's whole.
	FWorldMapFrame Cutscene = Latest;
	Cutscene.bPlayerView = false;
	BlendWorldMapFrames(Cutscene, nullptr, {}, 1.0f, Blend);
	UnroundWorldMapBlend(Cutscene, nullptr, 1.0f, Blend);
	CHECK(Blend.Moves[0] == UnmovedView);
}

TEST_CASE("A display between ticks moves the world map on to the player's view as it stands", "[WorldMap]")
{
	const FWorldMapCameraOffsets Before{1.0f, 0.0f, 10.0f};
	const FWorldMapCameraOffsets After{1.2f, 0.0f, 16.0f};
	const FWorldMapFrame Previous = FrameUnder(7, Before);
	const FWorldMapFrame Latest = FrameUnder(8, After);
	const FWorldMapCameraOffsets Live{1.3f, 0.0f, 19.0f};
	const FPoint InWorld{120.0f, -60.0f, -340.0f};
	const std::vector<int32> Matches{0, 1};
	FWorldMapFrameBlend Blend;

	// Shown as the latest frame draws it, the live view is the live offsets' camera.
	BlendWorldMapFrames(Latest, &Previous, Matches, 1.0f, Blend);
	REQUIRE(Blend.bBlended);
	MoveWorldMapBlendToView(Latest, &Previous, 1.0f, Live, Blend);
	const FPoint Drawn = Through(WorldMapPlayerViewAffine(AtRest, Latest.ShownOffsets), InWorld);
	CHECK(Apart(Moved(Blend.Moves[0], Drawn), Through(WorldMapPlayerViewAffine(AtRest, Live), InWorld)) < 0.05f);
	// The band scrolls by the turn the frame does not show yet: 4 of the yaw's units a texel.
	CHECK(Blend.HorizonScroll == Catch::Approx((19.0f - Latest.ShownOffsets.Turn) * 4096.0f / 360.0f / 4.0f).margin(0.01));

	// The sky's quads keep out of the turn: with no tilt between the two, they stay where they are but for the eye's move.
	const FPoint Ahead{0.0f, 0.0f, 1.0f};
	const FPoint SkyWay{Blend.Moves[1][0][2], Blend.Moves[1][1][2], Blend.Moves[1][2][2]};
	CHECK(Apart(SkyWay, Ahead) < 0.001f);
	const FPoint WorldWay{Blend.Moves[0][0][2], Blend.Moves[0][1][2], Blend.Moves[0][2][2]};
	CHECK(Apart(WorldWay, Ahead) > 0.01f);

	// An anchor the tick before did not have is drawn as the latest tick has it, and moved on from there: a tree coming
	// into view while the zoom changes stands where the live view puts it, not a part of a tick's zoom away.
	BlendWorldMapFrames(Latest, &Previous, std::vector<int32>{-1, 1}, 0.5f, Blend);
	REQUIRE(Blend.bBlended);
	MoveWorldMapBlendToView(Latest, &Previous, 0.5f, Live, Blend);
	CHECK(Apart(Moved(Blend.Moves[0], Drawn), Through(WorldMapPlayerViewAffine(AtRest, Live), InWorld)) < 0.05f);

	// Halfway between the ticks the blend shows the offsets halfway, and the move starts from there.
	BlendWorldMapFrames(Latest, &Previous, Matches, 0.5f, Blend);
	const float Scroll = Blend.HorizonScroll;
	MoveWorldMapBlendToView(Latest, &Previous, 0.5f, Live, Blend);
	const float ShownTurn = (Previous.ShownOffsets.Turn + Latest.ShownOffsets.Turn) / 2.0f;
	CHECK(Blend.HorizonScroll - Scroll == Catch::Approx((19.0f - ShownTurn) * 4096.0f / 360.0f / 4.0f).margin(0.01));

	// Nothing moves where the view stands as the frame shows it, or where the player's view does not apply.
	BlendWorldMapFrames(Latest, nullptr, {}, 1.0f, Blend);
	MoveWorldMapBlendToView(Latest, nullptr, 1.0f, Latest.ShownOffsets, Blend);
	CHECK(Blend.Moves[0] == UnmovedView);
	FWorldMapFrame Cutscene = Latest;
	Cutscene.bPlayerView = false;
	BlendWorldMapFrames(Cutscene, nullptr, {}, 1.0f, Blend);
	MoveWorldMapBlendToView(Cutscene, nullptr, 1.0f, Live, Blend);
	CHECK(Blend.Moves[0] == UnmovedView);
	CHECK(Blend.HorizonScroll == 0.0f);
}
