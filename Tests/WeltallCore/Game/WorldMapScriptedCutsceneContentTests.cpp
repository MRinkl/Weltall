#include "Foundation/LittleEndian.h"
#include "Game/FieldProgram.h"
#include "Game/GameState.h"
#include "Game/WorldMapProgram.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "WorldMap/WorldMapContext.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

// The second disc's world map cutscenes as the game reaches them: the narrator's stage (field map 617) hands each
// over after its narration, the world map plays it to its script's end, and the stage goes on by the entrance the
// cutscene names.

namespace
{
	constexpr uint16 StageMap = 617;

	/** The world map's words in the game state: the field's map, its entrance (the scene word as the world map leaves it). */
	constexpr std::size_t PlaceWord = 0x231A;
	constexpr std::size_t SceneWord = 0x2320;

	/** The entry's fade to black before the scene's first tick. */
	constexpr int32 EntryFrames = 66;

	/** The stage entered by Entrance at Story plays its narration, then world map scene Scene for Ticks ticks, and is entered again. */
	void PlayFromStage(const FIso9660& Disc, uint16 Story, uint16 Entrance, int32 Scene, int32 Ticks)
	{
		auto Game = Testing::FGameLoopSession::Start(Disc);
		Game->Session->Game().SetSavedEventVariable(0, Story);
		Game->Session->Game().SetFieldEntrance(Entrance);
		Game->EnterField(StageMap);
		// The narration: Cross closes each message, pressed every other frame.
		const FPadState Cross{FPadState::Bit(EPadButton::Cross)};
		int32 Frames = 0;
		while (Game->Loop->Program() != EGameProgram::WorldMap && Frames < 12000)
		{
			REQUIRE_FALSE(Game->Frame(Frames % 2 == 0 ? Cross : FPadState{}));
			++Frames;
		}
		REQUIRE(Game->Loop->Program() == EGameProgram::WorldMap);
		const FWorldMapProgram* WorldMap = Game->Loop->WorldMap();
		REQUIRE(WorldMap != nullptr);
		CHECK(WorldMap->Runtime().Context().SceneNumber == Scene);
		CHECK(WorldMap->Runtime().IsCutscene());
		const int32 Played = Game->RunUntil([&] { return Game->Loop->Program() != EGameProgram::WorldMap; }, EntryFrames + Ticks + 100);
		CHECK(Played == EntryFrames + Ticks);
		REQUIRE(Game->Loop->Program() == EGameProgram::Field);
		const FGameState& State = Game->Session->Game();
		CHECK(LittleEndian::ReadU16(State.Block(), PlaceWord) == StageMap);
		CHECK(LittleEndian::ReadU16(State.Block(), SceneWord) == Entrance);
		Game->RunUntil([&] { return Game->Loop->Field() != nullptr; }, 100);
		REQUIRE(Game->Loop->Field() != nullptr);
		CHECK(Game->Loop->Field()->Runtime().Context().RunningMap == StageMap);
	}
}

TEST_CASE("Field map 617's third entrance plays world map scene 17 to its script's end, then the stage goes on", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	PlayFromStage(*Disc, 252, 2, 17, 2299);
}

TEST_CASE("Field map 617's fifth entrance plays world map scene 18 to its script's end, then the stage goes on", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	PlayFromStage(*Disc, 285, 4, 18, 2235);
}
