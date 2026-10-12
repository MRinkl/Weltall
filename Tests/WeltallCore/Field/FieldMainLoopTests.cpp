#include "Field/FieldMainLoop.h"
#include "Field/FieldMapChange.h"
#include "Field/FieldPartySprites.h"
#include "Game/GameState.h"
#include "Foundation/LittleEndian.h"
#include "Support/ByteWriter.h"
#include "Support/FieldSongsModel.h"

#include <catch_amalgamated.hpp>

#include <memory>
#include <vector>

using namespace Weltall;
using Weltall::Testing::FByteWriter;

namespace
{
	/** A map of two script actors without code, the leader the second, and a game state. */
	struct FLoopMap
	{
		std::unique_ptr<FEventScripts> Scripts;
		FGameState Game;
		Testing::FFieldSongsModel Songs;
		FEventContext Context;

		FLoopMap()
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
			Context.States.resize(2);
			Context.LeaderActor = 1;
			Context.MapNumber = 0x40;
			Context.RunningMap = 0x40;
		}
	};

	/** The effect channels the field stops, in order. */
	class FStoppedChannels : public IFieldSoundEffects
	{
	public:
		std::vector<int32> Stopped;

		void StopEffectOn(int32 Channel) override { Stopped.push_back(Channel); }
		void PlayEffectOn(uint32, int32, int32, int32) override {}
		void SetEffectVolumeOn(int32, int32) override {}
		void SetEffectPanOn(int32, int32) override {}
		void FadeEffectVolumeOn(int32, int32, int32) override {}
		uint32 PlayingEffects() override { return 0; }
	};
}

TEST_CASE("The main loop takes up requests only while the field is quiet", "[Field]")
{
	FLoopMap Map;
	FEventContext& Context = Map.Context;
	CHECK(FieldMainLoop::IsIdle(Context));
	Map.Songs.bLoading = true;
	CHECK_FALSE(FieldMainLoop::IsIdle(Context));
	Map.Songs.bLoading = false;
	Context.MemberLoad.bPending = true;
	CHECK_FALSE(FieldMainLoop::IsIdle(Context));
	Context.MemberLoad.bPending = false;
	Context.bExtraAnimationsLoading = true;
	CHECK_FALSE(FieldMainLoop::IsIdle(Context));
	Context.bExtraAnimationsLoading = false;
	Context.Render.bEffectTexturesSaved = true;
	CHECK_FALSE(FieldMainLoop::IsIdle(Context));
}

TEST_CASE("A script's battle starts on a tick drawing into buffer 1, an encounter's once the leader stands", "[Field]")
{
	FLoopMap Map;
	FEventContext& Context = Map.Context;
	CHECK_FALSE(FieldMainLoop::BattleStarts(Context));
	Context.bBattleRequested = true;
	Context.DrawBuffer = 0;
	CHECK_FALSE(FieldMainLoop::BattleStarts(Context));
	Context.DrawBuffer = 1;
	CHECK(FieldMainLoop::BattleStarts(Context));
	Context.Render.bEffectTexturesSaved = true;
	CHECK_FALSE(FieldMainLoop::BattleStarts(Context));
	Context.Render.bEffectTexturesSaved = false;

	// An encounter does not wait for buffer 1, only for the leader to land (unless in the action mode).
	Context.Encounters.bBattleDrawn = true;
	Context.DrawBuffer = 0;
	Context.States[1].Flags0 = EventActorFlags0::Airborne;
	CHECK_FALSE(FieldMainLoop::BattleStarts(Context));
	Context.States[1].Flags0 = EventActorFlags0::Falling;
	CHECK(FieldMainLoop::BattleStarts(Context));
	Context.States[1].Flags0 = EventActorFlags0::Airborne;
	Context.Movement.ActionMode = 1;
	CHECK(FieldMainLoop::BattleStarts(Context));
}

TEST_CASE("A map change reads the next map's file and goes ahead a tick later, once the map's fade is at rest", "[Field]")
{
	FLoopMap Map;
	FEventContext& Context = Map.Context;
	CHECK_FALSE(FieldMainLoop::MapChangeGoesAhead(Context));
	Context.Requests.bMapChange = true;
	Context.MapNumber = 0x4000 | 0x123;
	Map.Songs.bLoading = true;
	CHECK_FALSE(FieldMainLoop::MapChangeGoesAhead(Context));
	CHECK(Context.MapFileRead == FEventContext::NoMapFile);
	Map.Songs.bLoading = false;
	CHECK_FALSE(FieldMainLoop::MapChangeGoesAhead(Context));
	CHECK(Context.MapFileRead == 0x123);
	Context.Render.Fades[0].Ticks = 3;
	CHECK_FALSE(FieldMainLoop::MapChangeGoesAhead(Context));
	Context.Render.Fades[0].Ticks = 0;
	CHECK(FieldMainLoop::MapChangeGoesAhead(Context));
	// A request for another map reads its file again.
	Context.MapNumber = 0x124;
	CHECK_FALSE(FieldMainLoop::MapChangeGoesAhead(Context));
	CHECK(FieldMainLoop::MapChangeGoesAhead(Context));
}

TEST_CASE("The world map, the arena and another program leave in that order, on ticks drawing into buffer 1", "[Field]")
{
	FLoopMap Map;
	FEventContext& Context = Map.Context;
	Context.Requests.bProgram = true;
	Context.Requests.bBattling = true;
	Context.Requests.bWorldMap = true;
	Context.DrawBuffer = 0;
	CHECK(FieldMainLoop::ProgramStarting(Context) == EFieldRequest::None);
	Context.DrawBuffer = 1;
	CHECK(FieldMainLoop::ProgramStarting(Context) == EFieldRequest::WorldMap);
	Context.Requests.bWorldMap = false;
	CHECK(FieldMainLoop::ProgramStarting(Context) == EFieldRequest::Battling);
	Context.Requests.bBattling = false;
	CHECK(FieldMainLoop::ProgramStarting(Context) == EFieldRequest::Program);
	Context.MemberLoad.bPending = true;
	CHECK(FieldMainLoop::ProgramStarting(Context) == EFieldRequest::None);
}

TEST_CASE("The player's buttons and the field's own screens wait while anything is asked for", "[Field]")
{
	FLoopMap Map;
	FEventContext& Context = Map.Context;
	CHECK(FieldMainLoop::TakesUpPlayer(Context));
	for (bool* Request : {&Context.Requests.bProgram, &Context.bBattleRequested, &Context.Requests.bWorldMap, &Context.Requests.bMapChange})
	{
		*Request = true;
		CHECK_FALSE(FieldMainLoop::TakesUpPlayer(Context));
		*Request = false;
	}
	// The arena asked for does not keep them waiting.
	Context.Requests.bBattling = true;
	CHECK(FieldMainLoop::TakesUpPlayer(Context));
}

TEST_CASE("Start pauses the field unless Cross is held or the scripts keep it from pausing", "[Field]")
{
	FLoopMap Map;
	FEventContext& Context = Map.Context;
	Context.Controls.Repeated = 0x0800;
	CHECK(FieldMainLoop::PausesOnStart(Context));
	Context.PadHeld = 0x0040;
	CHECK_FALSE(FieldMainLoop::PausesOnStart(Context));
	Context.PadHeld = 0;
	Context.PauseDisabled = 1;
	CHECK_FALSE(FieldMainLoop::PausesOnStart(Context));
}

TEST_CASE("The menu opens on ticks drawing into buffer 0 with the leader on the ground, and the player asks for it in control", "[Field]")
{
	FLoopMap Map;
	FEventContext& Context = Map.Context;
	Context.Controls.Repeated = 0x0010;
	Context.MenuParam = 1;
	CHECK_FALSE(FieldMainLoop::TakeMenuButton(Context));
	Context.bPlayerControlled = true;
	Context.bMenuLocked = true;
	CHECK_FALSE(FieldMainLoop::TakeMenuButton(Context));
	Context.bMenuLocked = false;
	REQUIRE(FieldMainLoop::TakeMenuButton(Context));
	CHECK(Context.MenuRequest == 0x80);
	CHECK(Context.MenuProgramParam == 1);
	CHECK_FALSE(FieldMainLoop::TakeMenuButton(Context));

	Context.DrawBuffer = 1;
	CHECK_FALSE(FieldMainLoop::MenuOpens(Context));
	Context.DrawBuffer = 0;
	Context.States[1].Flags0 = EventActorFlags0::Falling;
	CHECK_FALSE(FieldMainLoop::MenuOpens(Context));
	Context.States[1].Flags0 = 0;
	CHECK(FieldMainLoop::MenuOpens(Context));
}

TEST_CASE("The player's menu stays shut once the scripts lock it; theirs opens", "[Field]")
{
	FLoopMap Map;
	FEventContext& Context = Map.Context;
	Context.MenuRequest = 0x80;
	CHECK_FALSE(FieldMainLoop::MenuStaysShut(Context));
	Context.bMenuLocked = true;
	CHECK(FieldMainLoop::MenuStaysShut(Context));
	Context.MenuRequest = 1;
	CHECK_FALSE(FieldMainLoop::MenuStaysShut(Context));
}

TEST_CASE("Load Game's end hands the scripts the new game's map or the loaded game's, and freezes the per-tick update", "[Field]")
{
	FLoopMap Map;
	FEventContext& Context = Map.Context;
	FGameState& Game = Map.Game;
	const auto Var = [&](uint32 Address) { return Context.Variables.Get(Address, *Map.Scripts); };
	const auto Scene = [&] { return LittleEndian::ReadU16(Game.Block(), 0x2320); };
	Game.SetFieldEntrance(9);
	Game.SetSavedEventVariable(1, 9);
	Game.SetPlace(0x1EA);

	// Nothing after another menu, or after the title timed out.
	FieldMainLoop::TakeLoadGameResult(Context, 0, 0);
	FieldMainLoop::TakeLoadGameResult(Context, 2, 1);
	CHECK_FALSE(Context.bFrameVariablesFrozen);
	CHECK(Game.Place() == 0x1EA);

	// New Game: map 4 from its entrance 0.
	FieldMainLoop::TakeLoadGameResult(Context, 2, 0);
	CHECK(Context.bFrameVariablesFrozen);
	CHECK(Var(0x46) == 0);
	CHECK(Var(0x04) == 4);
	CHECK(Context.MapNumber == 4);
	CHECK(Game.Place() == 4);
	CHECK(Scene() == 0);
	CHECK(Game.SavedEventVariable(1) == 0);

	// A game loaded on a field map resumes at the entrance its variable 0x054 kept; on the world map the scene stays.
	FLoopMap Loaded;
	Loaded.Game.SetPlace(0xC000 | 0x123);
	Loaded.Game.SetSavedEventVariable(0x2A, 5);
	FieldMainLoop::TakeLoadGameResult(Loaded.Context, 0, 2);
	CHECK(Loaded.Context.bFrameVariablesFrozen);
	CHECK(Loaded.Context.Variables.Get(0x46, *Loaded.Scripts) == 2);
	CHECK(Loaded.Context.Variables.Get(0x04, *Loaded.Scripts) == 0x123);
	CHECK(LittleEndian::ReadU16(Loaded.Game.Block(), 0x2320) == 5);
	Loaded.Game.SetPlace(0x405);
	Loaded.Game.SetFieldEntrance(7);
	FieldMainLoop::TakeLoadGameResult(Loaded.Context, 0, 2);
	CHECK(Loaded.Context.Variables.Get(0x04, *Loaded.Scripts) == 0x405);
	CHECK(LittleEndian::ReadU16(Loaded.Game.Block(), 0x2320) == 7);
}

TEST_CASE("After the menu the gears of the slots whose riding it changed are boarded or left, in gear mode", "[Field]")
{
	FLoopMap Map;
	FEventContext& Context = Map.Context;
	FGameState& Game = Map.Game;
	Context.PartyActors = {1, 0, FEventContext::NoActor};
	Game.SetPartySlotInGear(0, 1);
	Game.SetPartySlotInGear(2, 1);
	const std::array<uint8, 3> Riding{0, 0, 0};
	CHECK(FieldMainLoop::GearsChangedByMenu(Context, Riding) == std::array<bool, 3>{});
	Context.PilotSpriteBase = 0x20;
	// Slot 2 has no actor.
	CHECK(FieldMainLoop::GearsChangedByMenu(Context, Riding) == std::array<bool, 3>{true, false, false});
}

TEST_CASE("L2 and R2 board or leave the gears once a press, where the field lets them", "[Field]")
{
	FLoopMap Map;
	FEventContext& Context = Map.Context;
	FGameState& Game = Map.Game;
	Context.PartyCharacters = {0, 1, FEventContext::NoActor};
	REQUIRE(Game.SetCharacterGear(0, 3));
	REQUIRE(Game.SetCharacterGear(1, 0xFF));
	Context.PadHeld = 0x0003;
	Context.bPlayerControlled = true;
	bool bHeld = false;
	std::array<bool, 3> Slots{};
	// Outside gear mode the gears cannot be boarded, but the press is taken, and the kernel's lock worked out at it.
	CHECK(FieldMainLoop::IsGearBoardingLocked(Context));
	CHECK_FALSE(FieldMainLoop::TakeGearBoarding(Context, bHeld, Slots));
	CHECK(bHeld);
	CHECK(Context.bGearsLocked);
	// The lock is worked out only at a new press.
	Context.PilotSpriteBase = 0x20;
	CHECK_FALSE(FieldMainLoop::TakeGearBoarding(Context, bHeld, Slots));
	CHECK(Context.bGearsLocked);
	Context.PadHeld = 0;
	CHECK_FALSE(FieldMainLoop::TakeGearBoarding(Context, bHeld, Slots));
	CHECK_FALSE(bHeld);
	Context.PadHeld = 0x0003;
	REQUIRE(FieldMainLoop::TakeGearBoarding(Context, bHeld, Slots));
	CHECK_FALSE(Context.bGearsLocked);
	CHECK(Slots == std::array<bool, 3>{true, false, false});
	// Riding, the slots riding go with the first; a riding byte other than 1 goes with neither.
	Context.PartyCharacters = {0, 1, 2};
	REQUIRE(Game.SetCharacterGear(1, 4));
	REQUIRE(Game.SetCharacterGear(2, 5));
	Game.SetPartySlotInGear(0, 2);
	Game.SetPartySlotInGear(1, 1);
	Game.SetPartySlotInGear(2, 0);
	Context.PadHeld = 0;
	CHECK_FALSE(FieldMainLoop::TakeGearBoarding(Context, bHeld, Slots));
	Context.PadHeld = 0x0003;
	REQUIRE(FieldMainLoop::TakeGearBoarding(Context, bHeld, Slots));
	CHECK(Slots == std::array<bool, 3>{false, true, false});
	Game.SetPartySlotInGear(0, 0);
	Game.SetPartySlotInGear(1, 0);

	// The leader's floor, or the scripts, forbid it.
	Context.States[1].FloorAttributes = 0x40;
	CHECK(FieldMainLoop::IsGearBoardingLocked(Context));
	Context.GearBoardingOverride = 0;
	CHECK_FALSE(FieldMainLoop::IsGearBoardingLocked(Context));
	Context.PilotSpriteBase = 0;
	Context.GearBoardingOverride = 1;
	CHECK(FieldMainLoop::IsGearBoardingLocked(Context));
}

TEST_CASE("Select shows the map's picture where the party holds the map's item, the leader marked on it", "[Field]")
{
	FLoopMap Map;
	FEventContext& Context = Map.Context;
	FGameState& Game = Map.Game;
	Context.Controls.Repeated = 0x0100;
	CHECK_FALSE(FieldMainLoop::SelectShowsPicture(Context));
	Context.bPlayerControlled = true;
	CHECK(FieldMainLoop::SelectShowsPicture(Context));
	Context.Requests.bMapChange = true;
	CHECK_FALSE(FieldMainLoop::SelectShowsPicture(Context));

	// Map 240 (in gears: the map word's high bits are left out) and its item, 61h, in a slot with a count.
	Context.MapNumber = 0x4000 | 240;
	CHECK_FALSE(FieldMainLoop::MapPicture(Context));
	Game.InventoryItems(0)[149] = 0x61;
	CHECK_FALSE(FieldMainLoop::MapPicture(Context));
	Game.InventoryCounts(0)[149] = 1;
	Context.States[1].Position = {-300 << 16, 7 << 16, 0x0123'8000};
	std::optional<FFieldMapPicture> Picture = FieldMainLoop::MapPicture(Context);
	REQUIRE(Picture);
	CHECK(Picture->File == 0x7FF);
	CHECK(Picture->HiddenParts == 0);
	// x: -300 x 2699 >> 16 = -13, less 4, plus 168; z: -(0x123 x 2674) >> 16 = -12, less 12, plus 135.
	CHECK(Picture->MarkerX == 151);
	CHECK(Picture->MarkerY == 111);
	Context.MapNumber = 238;
	CHECK_FALSE(FieldMainLoop::MapPicture(Context));

	// Map 706 hides the parts its flags (saved variable 0xE6, bits 8h to 80h) do not show: the last flag reads the other way.
	Game.InventoryItems(0)[0] = 0x62;
	Game.InventoryCounts(0)[0] = 3;
	Context.MapNumber = 706;
	Game.SetSavedEventVariable(0x73, 0x08 | 0x20);
	Picture = FieldMainLoop::MapPicture(Context);
	REQUIRE(Picture);
	CHECK(Picture->File == 0x801);
	CHECK(Picture->HiddenParts == (0x02 | 0x08));
	Game.SetSavedEventVariable(0x73, 0x80);
	CHECK(FieldMainLoop::MapPicture(Context)->HiddenParts == (0x01 | 0x02 | 0x04 | 0x08 | 0x10));
}

TEST_CASE("The field leaves for a battle with its place and the song it comes back to saved", "[Field]")
{
	FLoopMap Map;
	FEventContext& Context = Map.Context;
	FGameState& Game = Map.Game;
	FStoppedChannels Channels;
	Context.SoundEffects = &Channels;
	Context.ActorSounds.KeepMask = 0x2;
	Context.ActorSounds.Channels[0] = {1, 5};
	Context.Particles.Effects[3].bActive = true;
	Context.Messages.Windows[0].bOpen = true;
	Context.Encounters.Formation = 7;
	Context.Encounters.bBattleDrawn = true;
	Context.EncounterBattleKind = 3;
	Map.Songs.Song = 0x21;
	Context.Variables.Set(0x002, 4);
	FFieldRequest Request;
	Request.Kind = EFieldRequest::Battle;
	REQUIRE_FALSE(FieldMainLoop::LeaveField(Context, Request));
	CHECK(Request.NextProgram == 2);
	CHECK(Request.bFieldKept);
	CHECK(Request.Formation == 7);
	CHECK(Request.bEncounter);
	CHECK(Request.BattleKind == 3);
	CHECK(Request.BattleSong == FFieldEncounters::BattleSong);
	const std::span<const uint8> Block = Game.Block();
	CHECK(Game.Place() == 0x40);
	CHECK(LittleEndian::ReadU16(Block, 0x2322) == 0x21);
	CHECK(LittleEndian::ReadU16(Block, 0x2320) == 4);
	CHECK(Channels.Stopped == std::vector<int32>{0, 4, 6});
	CHECK(Context.ActorSounds.Channels[0].Owner == FFieldActorSounds::Free);
	CHECK_FALSE(Context.Particles.Effects[3].bActive);
	CHECK_FALSE(Context.Messages.Windows[0].bOpen);
	// The update of every tick ran once more: the clock stepped.
	CHECK(Context.Clock.Ticks == 1);
}

TEST_CASE("The field leaves for a scripted program from a new game's state", "[Field]")
{
	FLoopMap Map;
	FEventContext& Context = Map.Context;
	Context.Requests.Program = 0x83;
	Context.bFrameVariablesFrozen = true;
	FFieldRequest Request;
	Request.Kind = EFieldRequest::Program;
	REQUIRE_FALSE(FieldMainLoop::LeaveField(Context, Request));
	CHECK(Request.NextProgram == 3);
	CHECK(Request.bNewGame);
	CHECK_FALSE(Request.bFieldKept);
	// Frozen after a game's load, the update of every tick leaves the clock and the game state alone.
	CHECK(Context.Clock.Ticks == 0);
	CHECK(Map.Game.Place() == 0);
}

TEST_CASE("A map change keeps what the original's load leaves alone and starts the rest afresh", "[Field]")
{
	FEventContext Before;
	Before.PartyCharacters = {2, 5, FEventContext::NoActor};
	Before.Camera.EyeTo = {1, 2, 3};
	Before.Camera.ZoomRamp = {7, 0x1234, 0x10};
	Before.Camera.Pitch = 45;
	Before.Camera.FollowTicks = 9;
	Before.Encounters.Countdown = 40;
	Before.Encounters.Period = 60;
	Before.Encounters.Weights[3] = 8;
	Before.PadLatched = 0x20;
	Before.PadHeld = 0x10;
	Before.MessagesOpened = 1;
	Before.Particles.Setup[2].Count = 5;
	Before.Particles.Effects[0].bActive = true;
	Before.Clock.Ticks = 11;
	Before.Render.Fades[0].Step = {1, 2, 3};
	Before.Render.Fades[0].Ticks = 4;
	Before.Render.GearLights.Words[9] = 77;
	Before.Render.GearLights.Words[0] = 55;
	Before.Movement.JumpInterval = 6;
	Before.Movement.ActionMode = 1;
	Before.Controls.bTouchLatched = true;
	Before.MenusPending = 2;
	Before.Render.bEffectTexturesSaved = true;
	Before.Requests.bProgram = true;
	Before.Requests.Program = 5;
	Before.Requests.bMapChange = true;

	FEventContext After;
	KeepAcrossMaps(Before, After);
	CHECK(After.PartyCharacters == Before.PartyCharacters);
	CHECK(After.Camera.EyeTo == Before.Camera.EyeTo);
	CHECK(After.Camera.ZoomRamp.Value == 0x1234);
	CHECK(After.Camera.ZoomRamp.Ticks == 0);
	CHECK(After.Camera.Pitch == 30);
	CHECK(After.Camera.FollowTicks == 9);
	CHECK(After.Encounters.Countdown == 40);
	CHECK(After.Encounters.Period == 0);
	CHECK(After.Encounters.Weights[3] == 8);
	CHECK(After.PadLatched == 0x20);
	CHECK(After.PadHeld == 0);
	CHECK(After.MessagesOpened == 1);
	CHECK(After.Particles.Setup[2].Count == 5);
	CHECK_FALSE(After.Particles.Effects[0].bActive);
	CHECK(After.Clock.Ticks == 11);
	CHECK(After.Render.Fades[0].Step == std::array<int32, 3>{1, 2, 3});
	CHECK(After.Render.Fades[0].Ticks == 0);
	CHECK(After.Render.GearLights.Words[9] == 77);
	CHECK(After.Render.GearLights.Words[0] == 0x1F8);
	CHECK(After.Movement.JumpInterval == 6);
	CHECK(After.Movement.ActionMode == 0);
	CHECK(After.Controls.bTouchLatched);
	CHECK(After.MenusPending == 2);
	CHECK(After.Render.bEffectTexturesSaved);
	CHECK(After.Requests.bProgram);
	CHECK(After.Requests.Program == 5);
	CHECK_FALSE(After.Requests.bMapChange);
}
