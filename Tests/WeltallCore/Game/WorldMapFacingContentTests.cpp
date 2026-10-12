#include "Game/WorldMapProgram.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "WorldMap/WorldMapFrame.h"

#include <catch_amalgamated.hpp>

#include <cmath>
#include <vector>

using namespace Weltall;

// Which of the world map's triangles face the eye: the original tells on its screen and drops the rest; the frames of
// a free view leave it to the display, which knows where it draws each corner.

namespace
{
	constexpr FPadState NoButtons{};
	/** Era 1 (the story's value 30), at the scene's first entrance by Lahan: the plain's rises lie about. */
	constexpr uint16 Story = 30;
	constexpr uint16 Place = 0x401;
	constexpr uint16 Entrance = 1;
	constexpr uint16 CameraYaw = 0xC00;
	constexpr uint16 Scene = 1;
	constexpr int32 FadeInTicks = 62;

	struct FPlayedMap
	{
		std::unique_ptr<Testing::FGameLoopSession> Game;
		std::unique_ptr<FWorldMapProgram> Program;

		void Run(int32 Ticks = 1)
		{
			for (int32 Tick = 0; Tick < Ticks; ++Tick)
			{
				std::vector<FPadState> Vblanks(static_cast<std::size_t>(Program->VblanksPerFrame()), NoButtons);
				const std::optional<FError> Error = Program->Frame(Vblanks);
				INFO((Error ? Error->Message : std::string()));
				REQUIRE_FALSE(Error);
			}
		}
	};

	FPlayedMap EnterMap(const FIso9660& Disc, EWorldMapCulling Culling)
	{
		FPlayedMap Played;
		Played.Game = Testing::FGameLoopSession::Start(Disc);
		FGameState& State = Played.Game->Session->Game();
		State.SetSavedEventVariable(0, Story);
		State.SetWorldMapPlace(Place, Entrance, CameraYaw, Scene);
		auto Started = FWorldMapProgram::Start(*Played.Game->Session);
		INFO((Started ? std::string() : Started.Error().Message));
		REQUIRE(Started);
		Played.Program = std::move(*Started);
		Played.Program->SetCulling(Culling);
		Played.Run(FWorldMapProgram::EntryFadeFrames + FadeInTicks + 2);
		REQUIRE(Played.Program->LatestFrame() != nullptr);
		return Played;
	}

	/** Whether three view-space corners wind clockwise on the screen: seen from the eye, the triangle faces it. */
	bool FacesTheEye(const std::array<std::array<double, 3>, 3>& Corners)
	{
		const auto& [A, B, C] = Corners;
		return A[0] * (B[1] * C[2] - B[2] * C[1]) + A[1] * (B[2] * C[0] - B[0] * C[2]) + A[2] * (B[0] * C[1] - B[1] * C[0]) > 0.0;
	}

	struct FFacings
	{
		int32 Triangles = 0;
		int32 Away = 0;
		/** Facing the eye where they stand, away by the whole units the GTE keeps of their corners. */
		int32 AwayInWholeUnits = 0;
		int32 TwoSided = 0;
	};

	void Count(const FWorldMapFrame& Frame, EWorldMapLayer Layer, FFacings& Facings)
	{
		for (const FWorldMapPolygon& Polygon : Frame.Polygons)
		{
			if (Polygon.Layer != Layer)
			{
				continue;
			}
			std::array<std::array<double, 3>, 3> Exact{};
			std::array<std::array<double, 3>, 3> Whole{};
			for (std::size_t Corner = 0; Corner < 3; ++Corner)
			{
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Exact[Corner][Axis] = Polygon.View[Corner][Axis];
					Whole[Corner][Axis] = std::floor(Polygon.View[Corner][Axis]);
				}
			}
			++Facings.Triangles;
			Facings.TwoSided += Polygon.bOneSided ? 0 : 1;
			Facings.Away += FacesTheEye(Exact) ? 0 : 1;
			Facings.AwayInWholeUnits += FacesTheEye(Exact) && !FacesTheEye(Whole) ? 1 : 0;
		}
	}
}

TEST_CASE("A free view's frames leave the facing of the ground and the models to the display", "[Game][WorldMap][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FPlayedMap Viewed = EnterMap(*Disc, EWorldMapCulling::InRange);
	// Pulled back and tilted, then a whole turn, five degrees a tick.
	FFieldViewInput Back;
	Back.Zoom = -0.5f;
	Back.Tilt = 10.0f;
	Viewed.Program->AddViewInput(Back);
	FFacings Ground;
	FFacings Models;
	for (int32 Tick = 0; Tick < 72; ++Tick)
	{
		FFieldViewInput Turn;
		Turn.Turn = 5.0f;
		Viewed.Program->AddViewInput(Turn);
		Viewed.Run();
		Count(*Viewed.Program->LatestFrame(), EWorldMapLayer::Terrain, Ground);
		Count(*Viewed.Program->LatestFrame(), EWorldMapLayer::Model, Models);
	}
	INFO("ground: " << Ground.Triangles << " triangles, " << Ground.Away << " facing away, " << Ground.AwayInWholeUnits
		<< " facing the eye that whole units turn away; models: " << Models.Triangles << ", " << Models.Away << ", " << Models.AwayInWholeUnits);
	// Every triangle is the display's to drop where it faces away, and those that do are in the frame for it.
	CHECK(Ground.TwoSided == 0);
	CHECK(Models.TwoSided == 0);
	CHECK(Ground.Away > 0);
	CHECK(Models.Away > 0);
	// Told from the GTE's whole units some that face the eye would be gone: slivers at a crest, each a hairline through
	// which what lies behind the ground shows.
	CHECK(Ground.AwayInWholeUnits > 0);
}

TEST_CASE("The original's culling drops what faces away on its own screen", "[Game][WorldMap][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FPlayedMap Played = EnterMap(*Disc, EWorldMapCulling::Original);
	FFacings Ground;
	FFacings Models;
	Played.Run(10);
	Count(*Played.Program->LatestFrame(), EWorldMapLayer::Terrain, Ground);
	Count(*Played.Program->LatestFrame(), EWorldMapLayer::Model, Models);
	REQUIRE(Ground.Triangles > 100);
	REQUIRE(Models.Triangles > 0);
	CHECK(Ground.TwoSided == Ground.Triangles);
	CHECK(Models.TwoSided == Models.Triangles);
}
