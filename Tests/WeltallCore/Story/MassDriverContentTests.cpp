#include "Field/FieldMap.h"
#include "Game/FieldProgram.h"
#include "Game/WorldMapProgram.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "Support/PortStory.h"
#include "WorldMap/WorldMapContext.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <numbers>
#include <optional>
#include <string>

using namespace Weltall;

namespace
{
	/**
	 * Elly's errand to the Mass Driver in the expanded second disc, at the story's progress it runs at: out of Taura's
	 * house onto the world map, north to the ruins, down into the airlock, along the halls to the control room.
	 */
	constexpr uint16 ErrandProgress = 253;
	/** A later point of the story, after the launch, when the ruins are only ruins. */
	constexpr uint16 AfterLaunch = 300;
	constexpr uint16 TaurasRoomMap = 616;
	constexpr uint16 TaurasClearingMap = 614;
	constexpr uint16 AirlockMap = 732;
	constexpr uint16 HallsMap = 733;
	constexpr uint16 ControlRoomMap = 734;
	/** The disc map whose formations the halls take: a hall of Anima Relic 1. */
	constexpr uint16 GuardiansMap = 629;
	/** Where the clearing's way out leads: Taura's House's arrival on the world map (20); the ruins' own (40), beside them. */
	constexpr int32 TaurasHouseArrivalX = 28162;
	constexpr int32 TaurasHouseArrivalZ = 23793;
	constexpr int32 RuinsArrivalX = 29700;
	constexpr int32 RuinsArrivalZ = 16620;
	/** The Yggdrasil's place and flags as saves of the first disc's end and the second disc's start keep them. */
	constexpr int16 ParkedShipX = 28815;
	constexpr int16 ParkedShipZ = 23059;
	constexpr uint16 FlyingShipAboard = 0x4003;
	constexpr uint8 Elly = 1;
	constexpr uint8 Emeralda = 6;
	constexpr uint8 NoOne = 0xFF;
	constexpr int32 WalkFrames = 1500;

	constexpr uint16 Up = FPadState::Bit(EPadButton::Up);
	constexpr uint16 Right = FPadState::Bit(EPadButton::Right);
	constexpr uint16 Down = FPadState::Bit(EPadButton::Down);
	constexpr uint16 Left = FPadState::Bit(EPadButton::Left);
	constexpr uint16 Cross = FPadState::Bit(EPadButton::Cross);

	/** An angle of the game's (4096 a turn) of the way along Across and Along, as atan2 gives it. */
	int32 GameAngle(double Across, double Along)
	{
		return static_cast<int32>(std::lround(std::atan2(Across, Along) * 2048.0 / std::numbers::pi));
	}

	/**
	 * The d-pad buttons that walk the field's leader most nearly toward (X, Z): the field walks the cosine and the
	 * negated sine of a direction, each button's own (right 0, down a quarter turn, ...) less the view's yaw.
	 */
	uint16 FieldPadToward(const FEventContext& Context, int32 X, int32 Z)
	{
		static constexpr std::array<uint16, 8> ByEighth = {Right, Right | Down, Down, Down | Left, Left, Left | Up, Up, Up | Right};
		const FFieldPoint& At = Context.States[Context.LeaderActor].Position;
		const int32 Way = GameAngle(-(Z - (At[2] >> 16)), X - (At[0] >> 16));
		const int32 Pad = (Way + Context.Camera.GoalViewYaw + Context.ViewTurn) & 0xFFF;
		return ByEighth[static_cast<std::size_t>(((Pad + 0x100) >> 9) & 7)];
	}

	/** The leader's distance from (X, Z) on the field, in whole units. */
	double FieldDistance(const FEventContext& Context, int32 X, int32 Z)
	{
		const FFieldPoint& At = Context.States[Context.LeaderActor].Position;
		return std::hypot(X - (At[0] >> 16), Z - (At[2] >> 16));
	}

	/** Takes the field's random encounters off for a walk: their battles are no part of the way. */
	void LeaveOutEncounters(Testing::FGameLoopSession& Game)
	{
		if (FFieldProgram* Field = Game.Loop->Field())
		{
			FFieldEncounters& Encounters = Field->Runtime().Context().Encounters;
			Encounters.Period = 0;
			Encounters.Pending.fill(FFieldEncounters::NoEncounter);
		}
	}

	/** Walks the field's leader to within Near of each point in turn by the pad; the test fails where it cannot. */
	void WalkField(Testing::FGameLoopSession& Game, std::initializer_list<std::array<int32, 2>> Points, double Near = 12.0)
	{
		for (const auto& [X, Z] : Points)
		{
			const auto Arrived = [&Game, X, Z, Near]
			{
				const FFieldProgram* Field = Game.Loop->Field();
				REQUIRE(Field != nullptr);
				return FieldDistance(Field->Runtime().Context(), X, Z) <= Near;
			};
			int32 Frames = 0;
			while (!Arrived())
			{
				INFO("walking to " << X << ", " << Z);
				REQUIRE(Frames < WalkFrames);
				LeaveOutEncounters(Game);
				const std::optional<FError> Error = Game.Frame(FPadState{FieldPadToward(Game.Loop->Field()->Runtime().Context(), X, Z)});
				REQUIRE_FALSE(Error);
				++Frames;
			}
		}
	}

	/** Holds the pad toward (X, Z) and presses the confirm button at it, as at a door, until Done. */
	void ConfirmToward(Testing::FGameLoopSession& Game, int32 X, int32 Z, const std::function<bool()>& Done)
	{
		for (int32 Frame = 0; !Done(); ++Frame)
		{
			REQUIRE(Frame < WalkFrames);
			LeaveOutEncounters(Game);
			const FFieldProgram* Field = Game.Loop->Field();
			const uint16 Toward = Field != nullptr ? FieldPadToward(Field->Runtime().Context(), X, Z) : 0;
			const std::optional<FError> Error = Game.Frame(FPadState{static_cast<uint16>(Frame % 8 < 4 ? Toward | Cross : Toward)});
			REQUIRE_FALSE(Error);
		}
	}

	/** The field's running map, or none while another program runs. */
	std::optional<int32> FieldMap(const Testing::FGameLoopSession& Game)
	{
		const FFieldProgram* Field = Game.Loop->Field();
		return Field != nullptr ? std::optional<int32>(Field->Runtime().Context().RunningMap) : std::nullopt;
	}

	/** Until the field runs Map with the player in control of the party. */
	bool InControlOn(const Testing::FGameLoopSession& Game, int32 Map)
	{
		const FFieldProgram* Field = Game.Loop->Field();
		if (Field == nullptr)
		{
			return false;
		}
		const FEventContext& Context = Field->Runtime().Context();
		return Context.RunningMap == Map && Context.PlayerLock == 0 && !Context.bMenuLocked;
	}

	/** Where the world map has put the party's leader once it has settled after its start, in whole units (x, z). */
	std::array<int32, 2> WorldMapLeaderAfterStart(Testing::FGameLoopSession& Game)
	{
		constexpr int32 SettleFrames = 120;
		for (int32 Frame = 0; Frame < SettleFrames; ++Frame)
		{
			REQUIRE_FALSE(Game.Frame());
		}
		const FWorldMapProgram* WorldMap = Game.Loop->WorldMap();
		REQUIRE(WorldMap != nullptr);
		const FWorldMapContext& Context = WorldMap->Runtime().Context();
		CHECK(Context.Era == 8);
		const FWorldMapVector& Leader = Context.Tasks[FWorldMapContext::LeaderSlot].Position;
		return {Leader.X >> 12, Leader.Z >> 12};
	}

	/**
	 * A game of the expanded second disc on the first disc, as the player's game powers on, with the port's story and the
	 * Yggdrasil as the first disc's end leaves it on the world map: flying, the party aboard, parked by Taura's forest.
	 */
	std::unique_ptr<Testing::FGameLoopSession> ExpandedGame(const FIso9660& Disc, uint16 Progress)
	{
		auto Game = Testing::FGameLoopSession::Start(Disc);
		Game->Session->SetStory(&Testing::PortStory());
		REQUIRE_FALSE(Game->Session->InitNewGame());
		FGameState& State = Game->Session->Game();
		State.SetStoryMode(EStoryMode::Expanded);
		State.SetSavedEventVariable(0, Progress);
		State.SetParty({Elly, Emeralda, NoOne});
		State.SetWorldMapVehicle(ParkedShipX, 0, ParkedShipZ, 0);
		State.SetWorldMapFlags(FlyingShipAboard);
		return Game;
	}

	/** Enters field map Map by Entrance and runs until the player has the party there. */
	void EnterInControl(Testing::FGameLoopSession& Game, uint16 Map, uint16 Entrance)
	{
		FGameState& State = Game.Session->Game();
		State.SetSavedEventVariable(1, Entrance);
		State.SetFieldEntrance(Entrance);
		Game.EnterField(Map);
		constexpr int32 EntryFrames = 600;
		Game.RunUntil([&Game, Map] { return InControlOn(Game, Map); }, EntryFrames);
		REQUIRE(InControlOn(Game, Map));
	}
}

TEST_CASE("Past Elly's setting out, Taura's room entered by the entrance Fei woke by plays nothing, its Nanoreactors empty", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = ExpandedGame(*Disc, ErrandProgress);
	EnterInControl(*Game, TaurasRoomMap, 1);
	constexpr int32 Frames = 120;
	for (int32 Frame = 0; Frame < Frames; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame());
	}
	CHECK(InControlOn(*Game, TaurasRoomMap));
	const FEventContext& Context = Game->Loop->Field()->Runtime().Context();
	CHECK(Game->Session->Game().SavedEventVariable(0) == ErrandProgress);
	// Fei's and Elly's Nanoreactors (actors 12 and 13) and Citan (14) are gone.
	for (const int32 Actor : {12, 13, 14})
	{
		INFO("actor " << Actor);
		CHECK((Context.States[static_cast<std::size_t>(Actor)].Flags1 & EventActorFlags1::Removed) != 0);
	}
}

TEST_CASE("Taura's clearing opens onto the world map while Elly sets out, and stays shut in the original's story", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// West along the path from the house's door, past where the clearing's way out lies.
	constexpr std::array<int32, 2> PastTheDoor{-898, -108};
	constexpr std::array<int32, 2> OnThePath{-1250, -110};
	auto Game = ExpandedGame(*Disc, ErrandProgress);
	EnterInControl(*Game, TaurasClearingMap, 1);
	WalkField(*Game, {PastTheDoor});
	Game->RunUntil([&Game] { return Game->Loop->Program() == EGameProgram::WorldMap; }, WalkFrames,
		FPadState{FieldPadToward(Game->Loop->Field()->Runtime().Context(), OnThePath[0], OnThePath[1])});
	REQUIRE(Game->Loop->Program() == EGameProgram::WorldMap);
	CHECK(Game->Session->Game().Place() == 0x400);
	CHECK(WorldMapLeaderAfterStart(*Game) == std::array<int32, 2>{TaurasHouseArrivalX, TaurasHouseArrivalZ});

	auto Original = ExpandedGame(*Disc, ErrandProgress);
	Original->Session->Game().SetStoryMode(EStoryMode::Original);
	EnterInControl(*Original, TaurasClearingMap, 1);
	WalkField(*Original, {PastTheDoor, OnThePath}, 40.0);
	CHECK(FieldMap(*Original) == TaurasClearingMap);
}

TEST_CASE("The Mass Driver's ruins are entered on foot from the world map, into the airlock, and its lift leads back out beside them", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = ExpandedGame(*Disc, ErrandProgress);
	EnterInControl(*Game, AirlockMap, 0);
	// The lift, behind the party as it comes in: back up to the surface.
	ConfirmToward(*Game, 0, -145, [&Game] { return Game->Loop->Program() == EGameProgram::WorldMap; });
	FGameState& State = Game->Session->Game();
	CHECK(State.Place() == 0x400);
	CHECK(WorldMapLeaderAfterStart(*Game) == std::array<int32, 2>{RuinsArrivalX, RuinsArrivalZ});
	const FWorldMapContext& Context = Game->Loop->WorldMap()->Runtime().Context();
	// The Yggdrasil is with Bart: Elly and Emeralda walk, and none is shown; the game keeps the ship as it was.
	CHECK(Context.bVehicleAway);
	CHECK(Context.TravelMode == WorldMapTravel::OnFoot);
	// The minimap shows the ruins' own dot.
	CHECK(Context.Scene->StoryDots == 1u << 21);

	// North into the ruins' ground and the confirm button: the airlock, out of the lift.
	constexpr int32 InsideX = 29700;
	constexpr int32 InsideZ = 16540;
	for (int32 Frame = 0; Game->Loop->Program() == EGameProgram::WorldMap; ++Frame)
	{
		REQUIRE(Frame < WalkFrames);
		const FWorldMapVector& At = Context.Tasks[FWorldMapContext::LeaderSlot].Position;
		// The world map walks the sine and the negated cosine of a heading: up the view's yaw, right a quarter turn on.
		static constexpr std::array<uint16, 8> ByEighth = {Up, Up | Right, Right, Right | Down, Down, Down | Left, Left, Left | Up};
		const int32 Heading = GameAngle(InsideX - (At.X >> 12), -(InsideZ - (At.Z >> 12)));
		const int32 Yaw = (Context.CameraYaw + Context.ViewTurn) & 0xFFF;
		const bool bThere = std::hypot(InsideX - (At.X >> 12), InsideZ - (At.Z >> 12)) < 24.0;
		const uint16 Toward = bThere ? 0 : ByEighth[static_cast<std::size_t>((((Heading - Yaw) & 0xFFF) + 0x100) >> 9 & 7)];
		const bool bPress = bThere && Frame % 8 < 4;
		REQUIRE_FALSE(Game->Frame(FPadState{static_cast<uint16>(bPress ? Cross : Toward)}));
	}
	Game->RunUntil([&Game] { return InControlOn(*Game, AirlockMap); }, WalkFrames);
	CHECK(InControlOn(*Game, AirlockMap));
	CHECK(State.SavedEventVariable(1) == 0);
	CHECK(State.WorldMapFlags() == FlyingShipAboard);
	CHECK(State.WorldMapPosition()[0] == ParkedShipX);
}

TEST_CASE("Out of Taura's clearing in the original's story the party is aboard the Yggdrasil as the first disc left it", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = ExpandedGame(*Disc, ErrandProgress);
	FGameState& State = Game->Session->Game();
	State.SetStoryMode(EStoryMode::Original);
	State.SetWorldMapPlace(0x400, 20, 0xC00, 1);
	auto Started = FWorldMapProgram::Start(*Game->Session);
	REQUIRE(Started);
	const FWorldMapContext& Context = (*Started)->Runtime().Context();
	CHECK_FALSE(Context.bVehicleAway);
	CHECK(Context.TravelMode == WorldMapTravel::Flying);
	CHECK(Context.Scene->StoryDots == 0);
}

TEST_CASE("The Mass Driver's halls lead from the airlock to the control room, where Citan joins the party for the launch", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = ExpandedGame(*Disc, ErrandProgress);
	EnterInControl(*Game, AirlockMap, 0);
	// Round the lift's pillar to the east door.
	WalkField(*Game, {{150, -170}, {190, 0}});
	ConfirmToward(*Game, 220, 0, [&Game] { return InControlOn(*Game, HallsMap); });
	// The halls meet the ruin guardians of Anima Relic 1, whose formations the field holds, at the rate it rolls them.
	const FEventContext& Halls = Game->Loop->Field()->Runtime().Context();
	CHECK(Halls.Encounters.Period != 0);
	const auto Guardians = LoadFieldArchive(Disc->Image(), *Game->Index, GuardiansMap);
	REQUIRE(Guardians);
	const std::vector<uint8>& Formations = Guardians->Section(EFieldSection::Encounters);
	const auto& Kernel = Game->Session->Kernel().Formations;
	const std::size_t Kept = std::min(Formations.size(), Kernel.size());
	CHECK(std::equal(Formations.begin(), Formations.begin() + static_cast<std::ptrdiff_t>(Kept), Kernel.begin()));
	// Along the hall to its east door.
	WalkField(*Game, {{430, -575}});
	ConfirmToward(*Game, 441, -620, [&Game] { return FieldMap(*Game) == ControlRoomMap; });
	// The control room's scene takes the party: Citan joins it.
	constexpr int32 SceneFrames = 600;
	Game->RunUntil([&Game] { return Game->Session->Game().Party()[2] != NoOne; }, SceneFrames);
	CHECK(Game->Session->Game().Party() == std::array<uint8, 3>{Elly, Emeralda, 9});
	CHECK_FALSE(InControlOn(*Game, ControlRoomMap));
}

TEST_CASE("After the launch the Mass Driver's control room is quiet, and the halls lead back to the airlock", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = ExpandedGame(*Disc, AfterLaunch);
	EnterInControl(*Game, ControlRoomMap, 0);
	CHECK(Game->Session->Game().Party() == std::array<uint8, 3>{Elly, Emeralda, NoOne});
	// Out by the door, into the hall's east end, along it and through its west door into the airlock.
	Game->RunUntil([&Game] { return InControlOn(*Game, HallsMap); }, WalkFrames,
		FPadState{FieldPadToward(Game->Loop->Field()->Runtime().Context(), 233, 88)});
	REQUIRE(InControlOn(*Game, HallsMap));
	WalkField(*Game, {{-590, -571}});
	ConfirmToward(*Game, -617, -572, [&Game] { return InControlOn(*Game, AirlockMap); });
	CHECK(Game->Session->Game().SavedEventVariable(0) == AfterLaunch);
}
