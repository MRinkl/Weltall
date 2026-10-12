#include "Field/FieldMap.h"
#include "Foundation/LittleEndian.h"
#include "Game/FieldProgram.h"
#include "Game/GameState.h"
#include "Game/WorldMapProgram.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "WorldMap/WorldMapContext.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

// The world map's cutscene scenes 13, 15 and 16 as the game reaches them: the field map's script hands each over, the
// world map plays it to its end, and the field goes on in the map and by the entrance the scene names.

namespace
{
	/** The world map's words in the game state: the field's map, the field's entrance (the scene word as the world map leaves it). */
	constexpr std::size_t PlaceWord = 0x231A;
	constexpr std::size_t SceneWord = 0x2320;

	/** A flag of the field's scripts: a bit of a saved variable (the variable's address times 16 plus the bit). */
	void SetScriptFlag(FGameState& State, uint16 Flag)
	{
		const int32 Variable = (Flag >> 4) / 2;
		State.SetSavedEventVariable(Variable, static_cast<uint16>(State.SavedEventVariable(Variable) | 1u << (Flag & 0xF)));
	}

	/** The field played until the world map takes over, Cross pressed every other frame to close its messages. */
	void PlayFieldToWorldMap(Testing::FGameLoopSession& Game, int32 Limit)
	{
		const FPadState Cross{FPadState::Bit(EPadButton::Cross)};
		int32 Frames = 0;
		while (Game.Loop->Program() != EGameProgram::WorldMap && Frames < Limit)
		{
			REQUIRE_FALSE(Game.Frame(Frames % 2 == 0 ? Cross : FPadState{}));
			++Frames;
		}
		REQUIRE(Game.Loop->Program() == EGameProgram::WorldMap);
	}

	/** The cutscene played from the world map's start to the field map it leaves for: its scene, its ticks at least, and the map and entrance it hands the field. */
	void PlayCutsceneToField(Testing::FGameLoopSession& Game, int32 Scene, int32 Ticks, uint16 Map, uint16 Entrance)
	{
		const FWorldMapProgram* WorldMap = Game.Loop->WorldMap();
		REQUIRE(WorldMap != nullptr);
		CHECK(WorldMap->Runtime().Context().SceneNumber == Scene);
		CHECK(WorldMap->Runtime().IsCutscene());
		const int32 Played = Game.RunUntil([&] { return Game.Loop->Program() != EGameProgram::WorldMap; }, Ticks + 400);
		CHECK(Played >= Ticks);
		REQUIRE(Game.Loop->Program() == EGameProgram::Field);
		const FGameState& State = Game.Session->Game();
		CHECK(LittleEndian::ReadU16(State.Block(), PlaceWord) == Map);
		CHECK(LittleEndian::ReadU16(State.Block(), SceneWord) == Entrance);
		Game.RunUntil([&] { return Game.Loop->Field() != nullptr; }, 100);
		REQUIRE(Game.Loop->Field() != nullptr);
		CHECK(Game.Loop->Field()->Runtime().Context().RunningMap == Map);
	}
}

TEST_CASE("Field map 494's way out plays world map scene 13 to its end, then the field goes on in map 132", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	// The map before sets this flag; with it the map's way out to the world map plays the scene instead.
	constexpr uint16 SceneFlag = 0x10E9;
	constexpr uint8 WayOutZone = 1;
	SetScriptFlag(Game->Session->Game(), SceneFlag);
	Game->EnterField(494);
	Game->RunUntil([&] { return Game->Loop->Field() != nullptr; }, 100);
	REQUIRE(Game->Loop->Field() != nullptr);
	REQUIRE_FALSE(Game->Frame());
	// The leader put where the player walks it: the middle of the zone the map's way out watches.
	FEventContext& Field = Game->Loop->Field()->Runtime().Context();
	REQUIRE(Field.Zones.size() > WayOutZone);
	FEventActorState* Leader = Field.StateOf(Field.LeaderActor);
	REQUIRE(Leader != nullptr);
	int32 X = 0;
	int32 Z = 0;
	for (const FGteVector& Corner : Field.Zones[WayOutZone].Corners)
	{
		X += Corner[0];
		Z += Corner[2];
	}
	Leader->Position[0] = (X / 4) * 0x10000;
	Leader->Position[2] = (Z / 4) * 0x10000;
	PlayFieldToWorldMap(*Game, 600);
	PlayCutsceneToField(*Game, 13, 877, 132, 2);
}

TEST_CASE("Field map 483's exits play world map scene 15 by their entrances, then the field goes on in map 417", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	struct FCase
	{
		uint16 Flag;
		int32 Entrance;
		int32 Ticks;
		uint16 FieldEntrance;
	};
	// The map's last stage at this point of the story: by the flag the fight before left, the shot strikes and dives into the ground, or passes by.
	constexpr uint16 Story = 203;
	constexpr int32 StageVariable = 0x106 / 2;
	constexpr uint16 LastStage = 13;
	const FCase Cases[] = {{0x108A, 0, 851, 2}, {0x108B, 1, 568, 3}};
	for (const FCase& Case : Cases)
	{
		INFO("entrance " << Case.Entrance);
		auto Game = Testing::FGameLoopSession::Start(*Disc);
		Game->Session->Game().SetSavedEventVariable(0, Story);
		Game->Session->Game().SetSavedEventVariable(StageVariable, LastStage);
		SetScriptFlag(Game->Session->Game(), Case.Flag);
		Game->EnterField(483);
		PlayFieldToWorldMap(*Game, 6000);
		CHECK(Game->Loop->WorldMap()->Runtime().Context().Entrance == Case.Entrance);
		PlayCutsceneToField(*Game, 15, Case.Ticks, 417, Case.FieldEntrance);
	}
}

TEST_CASE("Field map 282's exit plays world map scene 16 to its end, then the field goes on in map 506", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	// The telegram from Shevat, at this point of the story.
	constexpr uint16 TelegramStory = 207;
	Game->Session->Game().SetSavedEventVariable(0, TelegramStory);
	Game->EnterField(282);
	PlayFieldToWorldMap(*Game, 6000);
	PlayCutsceneToField(*Game, 16, 836, 506, 0);
}
