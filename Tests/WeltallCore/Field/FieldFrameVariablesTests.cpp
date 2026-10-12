#include "Field/FieldFrameVariables.h"
#include "Game/GameState.h"
#include "Foundation/LittleEndian.h"
#include "Support/ByteWriter.h"
#include "Support/FieldSongsModel.h"
#include "Timing/PlayClock.h"

#include <catch_amalgamated.hpp>

#include <memory>

using namespace Weltall;
using Weltall::Testing::FByteWriter;

namespace
{
	/** A map whose scripts are two actors with no code: all the frame variables need of them. */
	struct FTwoActors
	{
		std::unique_ptr<FEventScripts> Scripts;
		FGameState Game;
		Testing::FFieldSongsModel Songs;
		FEventContext Context;

		FTwoActors()
		{
			FByteWriter Writer;
			for (int32 Word = 0; Word < 32; ++Word)
			{
				Writer.U32(0);
			}
			Writer.U32(2);
			for (int32 Entry = 0; Entry < 64; ++Entry)
			{
				Writer.U16(0);
			}
			auto Parsed = FEventScripts::Parse(Writer.Bytes);
			REQUIRE(Parsed);
			Scripts = std::make_unique<FEventScripts>(std::move(*Parsed));
			Context.Scripts = Scripts.get();
			Context.GameState = &Game;
			Context.Songs = &Songs;
			Context.MapNumber = 0xC123;
			Context.States.resize(2);
		}
	};
}

TEST_CASE("The play clock counts vblanks into seconds, minutes and hours and stops at 100 hours", "[Timing]")
{
	FPlayClock Clock;
	for (int32 Vblank = 0; Vblank < 60 * 61; ++Vblank)
	{
		Clock.Vblank();
	}
	CHECK(Clock.Frames == 0);
	CHECK(Clock.Seconds == 1);
	CHECK(Clock.Minutes == 1);

	FPlayClock Late{59, 59, 59, 99, false};
	Late.Vblank();
	CHECK(Late.Hours == 100);
	CHECK(Late.bStopped);
	Late.Vblank();
	CHECK(Late.Frames == 0);
}

TEST_CASE("Every tick the field reports the play clock and the leader, and saves the party and the story", "[Field]")
{
	FTwoActors Map;
	FEventContext& Context = Map.Context;
	Context.PadHeld = 0x1020;
	Context.PadLatched = 0x0004;
	Context.PartyCharacters = {3, 0, FEventContext::NoActor};
	Context.LeaderActor = 1;
	Context.States[1].Position = {-5 << 16 | 0x8000, 12 << 16, 700 << 16};
	Context.Variables.Set(0x010, 0x1234);
	Context.Variables.Set(0x400, 0x5678);
	REQUIRE_FALSE(UpdateFieldFrameVariables(Context, FPlayClock{0, 7, 3, 2, false}));
	CHECK(Context.PadLatched == 0x1024);
	CHECK(Map.Game.Party() == std::array<uint8, 3>{3, 0, 0xFF});
	CHECK(Map.Game.SavedEventVariable(0x010 / 2) == 0x1234);
	CHECK(Context.Variables.Raw(0x00C) == 0x0307);
	CHECK(Context.Variables.Raw(0x00E) == 2);
	CHECK(Context.Variables.Raw(0x01E) == static_cast<uint16>(-5));
	CHECK(Context.Variables.Raw(0x020) == 700);
	CHECK(Context.Variables.Raw(0x022) == 12);
	CHECK(Context.Clock.Ticks == 1);
}

TEST_CASE("Every tick the field saves its place and tells its scripts about the game around them", "[Field]")
{
	FTwoActors Map;
	FEventContext& Context = Map.Context;
	FGameState& Game = Map.Game;
	Context.PartyCharacters = {3, 0, FEventContext::NoActor};
	Context.LeaderActor = 1;
	Context.States[1].Facing = 0x0400;
	Context.Camera.GoalViewYaw = 0;
	Context.Camera.Pitch = -20;
	Map.Songs.Song = 0x21;
	Game.ProgramResult = 1;
	Game.BattleActingTurns = 5;
	// The place takes the entrance and the camera's direction the tick before saved.
	Game.SetSavedEventVariable(0x002 / 2, 7);
	Game.SetSavedEventVariable(0x008 / 2, 3);
	Context.Variables.Set(0x002, 9);
	REQUIRE_FALSE(UpdateFieldFrameVariables(Context, FPlayClock{}));
	const std::span<const uint8> Block = Game.Block();
	CHECK(Game.Place() == 0xC123);
	CHECK(LittleEndian::ReadU16(Block, 0x231C) == 3 << 9);
	CHECK(LittleEndian::ReadU16(Block, 0x2320) == 7);
	CHECK(LittleEndian::ReadU16(Block, 0x2322) == 0x21);
	CHECK(Context.Variables.Raw(0x044) == 5);
	CHECK(Context.Variables.Raw(0x046) == 1);
	CHECK(Context.Variables.Raw(0x006) == 4);
	// A goal yaw of 0 counts as eighth -1, and 7 - (-1) wraps to 0.
	CHECK(Context.Variables.Raw(0x008) == 0);
	CHECK(Context.Variables.Raw(0x024) == static_cast<uint16>(-20));
	CHECK(Context.Variables.Raw(0x03C) == 0xC123);
	CHECK(Context.Variables.Raw(0x03E) == 3);
	CHECK(Context.Variables.Raw(0x040) == 0);
	CHECK(Context.Variables.Raw(0x042) == 0xFF);
	// The variables are saved after they are reported.
	CHECK(Game.SavedEventVariable(0x002 / 2) == 9);
	CHECK(Game.SavedEventVariable(0x03C / 2) == 0xC123);
	Context.Camera.GoalViewYaw = 0x0600;
	REQUIRE_FALSE(UpdateFieldFrameVariables(Context, FPlayClock{}));
	CHECK(Context.Variables.Raw(0x008) == 5);
	CHECK(LittleEndian::ReadU16(Block, 0x2320) == 9);
}

TEST_CASE("A party slot riding its gear records where its pilot is", "[Field]")
{
	FTwoActors Map;
	FEventContext& Context = Map.Context;
	Context.PartyActors = {0, FEventContext::NoActor, 1};
	Context.States[1].Layer = 2;
	Context.States[1].Position = {100 << 16, 0, -40 << 16};
	Map.Game.SetPartySlotInGear(2, true);
	Map.Game.SetPartySlotInGear(1, true);
	REQUIRE_FALSE(UpdateFieldFrameVariables(Context, FPlayClock{}));
	CHECK(Context.Variables.Raw(0x036) == (0x123 | 2 << 14));
	CHECK(Context.Variables.Raw(0x038) == 100);
	CHECK(Context.Variables.Raw(0x03A) == static_cast<uint16>(-40));
	// Slot 0 rides nothing, slot 1 is empty: their places are untouched.
	CHECK(Context.Variables.Raw(0x02A) == 0);
	CHECK(Context.Variables.Raw(0x030) == 0);
}

TEST_CASE("The frame variables report a leader without a script state", "[Field]")
{
	FTwoActors Map;
	Map.Context.LeaderActor = 5;
	CHECK(UpdateFieldFrameVariables(Map.Context, FPlayClock{}).has_value());
}
