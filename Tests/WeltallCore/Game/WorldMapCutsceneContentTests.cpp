#include "Foundation/LittleEndian.h"
#include "Game/FieldProgram.h"
#include "Game/GameState.h"
#include "Game/WorldMapProgram.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "WorldMap/WorldMapContext.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

// A world map cutscene as the game reaches it: the field's script hands it over, the world map plays it to its end,
// and the field goes on in the map the cutscene names.

namespace
{
	/** Bart's escape from the Goliath Factory: map 271 leaves for scene 10 at this point of the story. */
	constexpr uint16 EscapeMap = 271;
	constexpr uint16 EscapeStory = 138;
	constexpr uint16 ArrivalMap = 272;

	/** The world map's words in the game state: the field's map, its entrance (the scene word as the world map leaves it). */
	constexpr std::size_t PlaceWord = 0x231A;
	constexpr std::size_t SceneWord = 0x2320;
}

TEST_CASE("Field map 271's exit plays world map scene 10 to its end, then the field goes on in map 272", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->Session->Game().SetSavedEventVariable(0, EscapeStory);
	Game->EnterField(EscapeMap);
	// The map's script talks: Cross closes each message, pressed every other frame.
	const FPadState Cross{FPadState::Bit(EPadButton::Cross)};
	int32 Frames = 0;
	while (Game->Loop->Program() != EGameProgram::WorldMap && Frames < 4000)
	{
		REQUIRE_FALSE(Game->Frame(Frames % 2 == 0 ? Cross : FPadState{}));
		++Frames;
	}
	REQUIRE(Game->Loop->Program() == EGameProgram::WorldMap);
	const FWorldMapProgram* WorldMap = Game->Loop->WorldMap();
	REQUIRE(WorldMap != nullptr);
	CHECK(WorldMap->Runtime().Context().SceneNumber == 10);
	CHECK(WorldMap->Runtime().IsCutscene());
	// The screen's fade (66 frames), then the scene's 806 ticks.
	const int32 Played = Game->RunUntil([&] { return Game->Loop->Program() != EGameProgram::WorldMap; }, 2000);
	CHECK(Played >= 806);
	REQUIRE(Game->Loop->Program() == EGameProgram::Field);
	const FGameState& State = Game->Session->Game();
	CHECK(LittleEndian::ReadU16(State.Block(), PlaceWord) == ArrivalMap);
	CHECK(LittleEndian::ReadU16(State.Block(), SceneWord) == 0);
	Game->RunUntil([&] { return Game->Loop->Field() != nullptr; }, 100);
	REQUIRE(Game->Loop->Field() != nullptr);
	CHECK(Game->Loop->Field()->Runtime().Context().RunningMap == ArrivalMap);
}
