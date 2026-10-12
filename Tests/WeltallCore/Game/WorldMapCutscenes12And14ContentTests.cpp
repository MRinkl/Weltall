#include "Foundation/LittleEndian.h"
#include "Game/FieldProgram.h"
#include "Game/GameState.h"
#include "Game/WorldMapProgram.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "WorldMap/WorldMapContext.h"
#include "WorldMap/WorldMapFrame.h"

#include <catch_amalgamated.hpp>

#include <algorithm>

using namespace Weltall;

// World map scenes 12 and 14 as the game reaches them: the field hands each over, the world map plays it to its end,
// and the field goes on in the map the cutscene names.

namespace
{
	/** The Yggdrasil's bridge as the fleet is sighted (the map's entrance 11): its script leaves for scene 12. */
	constexpr uint16 BridgeMap = 290;
	constexpr uint16 SightingEntrance = 11;
	constexpr uint16 FleetArrivalMap = 273;
	constexpr uint16 FleetArrivalEntrance = 2;
	constexpr int32 FleetTicks = 1463;

	/** Map 505 leaves for scene 14 after its battle, with these words. */
	constexpr uint16 BlastMap = 505;
	constexpr uint16 BlastArrivalMap = 282;
	constexpr int32 BlastTicks = 667;

	/** The world map's words in the game state: the field's map and its entrance (the scene word as the world map leaves it). */
	constexpr std::size_t PlaceWord = 0x231A;
	constexpr std::size_t SceneWord = 0x2320;

	uint16 Word(const Testing::FGameLoopSession& Game, std::size_t Offset)
	{
		return LittleEndian::ReadU16(Game.Session->Game().Block(), Offset);
	}

	/** The cutscene played from the world map's start to the field's map running; the frames it took. */
	int32 PlayCutscene(Testing::FGameLoopSession& Game, int32 Scene, uint16 ArrivalMap, uint16 ArrivalEntrance)
	{
		REQUIRE(Game.Loop->Program() == EGameProgram::WorldMap);
		const FWorldMapProgram* WorldMap = Game.Loop->WorldMap();
		REQUIRE(WorldMap != nullptr);
		CHECK(WorldMap->Runtime().Context().SceneNumber == Scene);
		CHECK(WorldMap->Runtime().IsCutscene());
		const int32 Played = Game.RunUntil([&] { return Game.Loop->Program() != EGameProgram::WorldMap; }, 3000);
		REQUIRE(Game.Loop->Program() == EGameProgram::Field);
		CHECK(Word(Game, PlaceWord) == ArrivalMap);
		CHECK(Word(Game, SceneWord) == ArrivalEntrance);
		Game.RunUntil([&] { return Game.Loop->Field() != nullptr; }, 100);
		REQUIRE(Game.Loop->Field() != nullptr);
		CHECK(Game.Loop->Field()->Runtime().Context().RunningMap == ArrivalMap);
		return Played;
	}
}

TEST_CASE("Field map 290's exit plays world map scene 12 to its end, then the field goes on in map 273", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->Session->Game().SetFieldEntrance(SightingEntrance);
	Game->EnterField(BridgeMap);
	// The bridge's crew talks: Cross closes each message, pressed every other frame.
	const FPadState Cross{FPadState::Bit(EPadButton::Cross)};
	int32 Frames = 0;
	while (Game->Loop->Program() != EGameProgram::WorldMap && Frames < 6000)
	{
		REQUIRE_FALSE(Game->Frame(Frames % 2 == 0 ? Cross : FPadState{}));
		++Frames;
	}
	// The screen's fade (66 frames), then the scene's ticks.
	CHECK(PlayCutscene(*Game, 12, FleetArrivalMap, FleetArrivalEntrance) >= FleetTicks);
}

TEST_CASE("Map 505's exit to world map scene 14 plays it to its end, then the field goes on in map 282", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->EnterField(BlastMap);
	Game->RunUntil([&] { return Game->Loop->Field() != nullptr && Game->Loop->Field()->Runtime().Context().bFieldRunning; }, 400);
	// The map's script fights a battle before it leaves: the test hands over what its exit does, the battle not fought.
	FEventContext& Context = Game->Loop->Field()->Runtime().Context();
	Context.Requests.bWorldMap = true;
	Context.bFrameVariablesFrozen = true;
	Game->Session->Game().SetWorldMapPlace(0x400, 0, 0x800, 14);
	Game->RunUntil([&] { return Game->Loop->Program() == EGameProgram::WorldMap; }, 4);
	CHECK(PlayCutscene(*Game, 14, BlastArrivalMap, 0) >= BlastTicks);
}

TEST_CASE("Scene 14's grown rings are left out where the original's horizon band covers them, whatever the culling", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->EnterField(BlastMap);
	Game->RunUntil([&] { return Game->Loop->Field() != nullptr && Game->Loop->Field()->Runtime().Context().bFieldRunning; }, 400);
	FEventContext& Context = Game->Loop->Field()->Runtime().Context();
	Context.Requests.bWorldMap = true;
	Context.bFrameVariablesFrozen = true;
	Game->Session->Game().SetWorldMapPlace(0x400, 0, 0x800, 14);
	Game->RunUntil([&] { return Game->Loop->Program() == EGameProgram::WorldMap; }, 4);
	Game->Loop->WorldMap()->SetCulling(EWorldMapCulling::InRange);
	// The cullings for wider views draw the sky this many entries farther back than the original links it.
	constexpr uint16 SkyBehind = 0x1000;
	// The near view after the fly-by: the four rings full grown about the camera.
	int32 Checked = 0;
	for (int32 Frame = 0; Frame < 600 && Game->Loop->Program() == EGameProgram::WorldMap; ++Frame)
	{
		Game->Frame(FPadState{});
		const FWorldMapFrame* Latest = Game->Loop->WorldMap()->LatestFrame();
		if (Frame < 450 || Latest == nullptr)
		{
			continue;
		}
		const auto Band = std::find_if(Latest->Polygons.begin(), Latest->Polygons.end(),
			[](const FWorldMapPolygon& Polygon) { return Polygon.Layer == EWorldMapLayer::Horizon; });
		if (Band == Latest->Polygons.end() || Band->Bucket >= SkyBehind + 0x3FF)
		{
			continue;
		}
		++Checked;
		for (const FWorldMapPolygon& Polygon : Latest->Polygons)
		{
			if (Polygon.Anchor == FWorldMapPolygon::NoAnchor)
			{
				continue;
			}
			const FWorldMapAnchor& Anchor = Latest->Anchors[Polygon.Anchor];
			const uint32 Object = Anchor.Key & 0xFFFFFF;
			if (Anchor.Kind() == EWorldMapAnchorKind::Model && Object >= 4 && Object <= 7)
			{
				INFO("frame " << Frame << ", object " << Object);
				CHECK(Polygon.Bucket <= Band->Bucket - SkyBehind);
			}
		}
	}
	CHECK(Checked > 0);
}
