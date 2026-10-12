#include "Field/FieldReturn.h"
#include "Field/FieldRuntime.h"
#include "Game/FieldProgram.h"
#include "Game/KernelState.h"
#include "Support/FieldTickSession.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "Support/GameText.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <optional>

using namespace Weltall;

// The field leaving for a battle and starting again after it (FFieldKernelState::Saved,
// FFieldRuntime::EnterMapAfterBattle), on map 363, whose scripts start a battle a few seconds in with the
// tick oracle's pad: what the field saves, when it comes back to it, and when it starts afresh; and the
// Field program coming back as the kernel runs it again. The tick oracle checks the return itself tick
// for tick.
namespace
{
	constexpr int32 BattleMap = 363;
	constexpr int32 Frames = 96;
	/** A variable the map's init sets to -1, which nothing changes before the battle. */
	constexpr uint32 InitVariable = 0x144;
	constexpr uint16 VillageMap = 35;
	constexpr uint32 ScratchVariable = 0x7F0;
	constexpr int32 ScratchValue = 0x5A5A;

	struct FBattleRun
	{
		Testing::FOracleTickSession& Session;
		FFieldRuntime& Runtime;

		/** Frames of the pad's script until the field stops for a battle; the frames run. */
		int32 RunToBattle(bool bScriptsNameNextMap)
		{
			int32 Frame = 0;
			for (const Testing::FOraclePadStep& Step : Testing::OraclePadScript(BattleMap, Frames))
			{
				Runtime.Context().Requests.bBattleNamesNextMap = bScriptsNameNextMap;
				REQUIRE_FALSE(Runtime.Tick(Testing::OracleVblanks(Step, Runtime.VblanksPerTick())));
				++Frame;
				if (Runtime.Request().Kind != EFieldRequest::None)
				{
					break;
				}
			}
			REQUIRE(Runtime.Request().Kind == EFieldRequest::Battle);
			return Frame;
		}
	};
}

TEST_CASE("The field saves itself for a battle and comes back to the state it saved", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::optional<Testing::FOracleFieldFiles> Files;
	REQUIRE(Testing::LoadOracleFieldFiles(*Disc, Files));
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	Testing::FOracleTickSession Session(*Files);
	FFieldRuntime Runtime(Session.Services(*Files, Resident));
	REQUIRE_FALSE(Runtime.EnterMap(BattleMap));
	CHECK_FALSE(Runtime.KernelState().Saved);
	FBattleRun Run{Session, Runtime};
	Run.RunToBattle(false);

	CHECK(Runtime.Request().bFieldKept);
	const FFieldKernelState Kernel = Runtime.KernelState();
	REQUIRE(Kernel.Saved);
	const FFieldSnapshot& Saved = *Kernel.Saved;
	REQUIRE(Saved.Actors.size() == Runtime.Context().States.size());
	const uint8 Leader = Saved.LeaderActor;
	const std::array<int32, 3> LeaderPosition = Saved.Actors[Leader].State.Position;
	CHECK(Saved.Variables.Raw(InitVariable) == 0xFFFF);
	CHECK(Saved.PartySpriteFiles[0] == Session.Party.File(0));
	CHECK(Kernel.Clock.Mode == Runtime.Context().Clock.Mode);

	// The battle over, the game starts the field again where the game state says it is, with the song it left for.
	Session.Songs.Song = Session.Game.FieldSong();
	REQUIRE_FALSE(Runtime.EnterMapAfterBattle(Session.Game.Place(), Kernel));
	const FEventContext& Context = Runtime.Context();
	CHECK(Context.LeaderActor == Leader);
	CHECK(Context.States[Leader].Position[0] == LeaderPosition[0]);
	CHECK(Context.States[Leader].Position[2] == LeaderPosition[2]);
	CHECK(Context.Variables.Raw(InitVariable) == 0xFFFF);
	CHECK_FALSE(Context.bReturning);
	CHECK(Session.Songs.Song == Session.Game.FieldSong());
}

TEST_CASE("A battle the scripts send on to another map saves nothing: the field starts afresh after it", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::optional<Testing::FOracleFieldFiles> Files;
	REQUIRE(Testing::LoadOracleFieldFiles(*Disc, Files));
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	Testing::FOracleTickSession Session(*Files);
	FFieldRuntime Runtime(Session.Services(*Files, Resident));
	REQUIRE_FALSE(Runtime.EnterMap(BattleMap));
	FBattleRun Run{Session, Runtime};
	Run.RunToBattle(true);
	CHECK_FALSE(Runtime.Request().bFieldKept);
	const FFieldKernelState Kernel = Runtime.KernelState();
	CHECK_FALSE(Kernel.Saved);

	// Nothing to come back to.
	CHECK(Runtime.EnterMapAfterBattle(BattleMap, Kernel));

	// The field clock goes on from where the field left it, whatever map the field starts on (the entry's
	// frame variables count a tick).
	REQUIRE_FALSE(Runtime.EnterMap(BattleMap, Kernel));
	CHECK(Runtime.Context().Clock.Ticks == Kernel.Clock.Ticks + 1);
	CHECK(Runtime.Context().Clock.Mode == Kernel.Clock.Mode);
	CHECK(Runtime.Context().Variables.Raw(InitVariable) == 0xFFFF);
}

TEST_CASE("The Field program run again after the battle it left for comes back to the map as it left it", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->EnterField(VillageMap);
	FFieldProgram* Field = Game->Loop->Field();
	REQUIRE(Field != nullptr);
	Game->RunUntil([&] { return Field->Runtime().Context().bFieldRunning; }, 100);
	Game->Frame();
	FEventContext& Context = Field->Runtime().Context();
	const uint8 Leader = Context.LeaderActor;
	// A variable of the map's half, which a fresh start clears and no script of the map uses.
	Context.Variables.Set(ScratchVariable, ScratchValue);
	Context.bBattleRequested = true;
	Context.Requests.bScriptedBattle = true;
	Context.Requests.bLeavingField = true;
	Game->RunUntil([&] { return Game->Loop->Program() == EGameProgram::Battle; }, 4);
	FKernelState& Kernel = Game->Session->Kernel();
	REQUIRE(Kernel.FieldTrips == 1);
	REQUIRE(Kernel.Field.Saved);
	const FFieldSnapshot Saved = *Kernel.Field.Saved;

	// The battle won (its program left where it began), the kernel runs the field again at the place it left.
	Game->Loop.reset();
	FBattleProgramEnd End;
	End.Result = FKernelState::BattleWon;
	Kernel.EndBattle(End, Game->Session->Game());
	REQUIRE(Kernel.NextProgram == EGameProgram::Field);
	Game->EnterField(static_cast<uint16>(Game->Session->Game().Place()));
	REQUIRE(Game->Loop->Stage() == EGameLoopStage::Program);
	Field = Game->Loop->Field();
	REQUIRE(Field != nullptr);
	CHECK(Kernel.FieldTrips == 0);
	const FEventContext& Back = Field->Runtime().Context();
	CHECK(Back.RunningMap == VillageMap);
	CHECK(Back.LeaderActor == Leader);
	CHECK(Back.Variables.Raw(ScratchVariable) == ScratchValue);
	// Every actor where it was saved, but the party's, which the load's layer steps place again.
	REQUIRE(Back.States.size() == Saved.Actors.size());
	for (std::size_t Actor = 0; Actor < Back.States.size(); ++Actor)
	{
		if (std::find(Back.PartyActors.begin(), Back.PartyActors.end(), static_cast<uint8>(Actor)) == Back.PartyActors.end())
		{
			INFO("actor " << Actor);
			CHECK(Back.States[Actor].Position == Saved.Actors[Actor].State.Position);
		}
	}
	CHECK(Back.States[Leader].Position[0] == Saved.Actors[Leader].State.Position[0]);
	CHECK(Back.States[Leader].Layer == Saved.Actors[Leader].State.Layer);
	Game->RunUntil([&] { return Field->Runtime().Context().bFieldRunning; }, 100);
	CHECK(Game->Loop->Stage() == EGameLoopStage::Program);
}
