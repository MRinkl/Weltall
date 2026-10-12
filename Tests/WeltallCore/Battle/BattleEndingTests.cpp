#include "Battle/BattleContext.h"
#include "Battle/BattleEnding.h"
#include "Game/GameState.h"

#include <catch_amalgamated.hpp>

#include <memory>

using namespace Weltall;

namespace
{
	/** A battle whose loop has stopped on End, with or without an event. */
	struct FEndedBattle
	{
		FGameState State;
		std::unique_ptr<FBattleContext> Context = std::make_unique<FBattleContext>();

		FEndedBattle(uint8 End, bool bEventBattle, const FBattleEventOptions& Options)
		{
			Context->GameState = &State;
			Context->EventOptions = Options;
			Context->bEventBattle = bEventBattle;
			Context->End = End;
			State.ProgramResult = 0x7E;
		}

		void Run()
		{
			BeginBattleEnding(*Context);
			int32 Frames = 0;
			while (StepBattleEnding(*Context))
			{
				REQUIRE(++Frames < 100);
			}
			REQUIRE(Context->Fault == EBattleFault::None);
		}
	};
}

TEST_CASE("A battle's end gives the mode's result and the pose by how it ended", "[Battle]")
{
	FEndedBattle Won(BattleEnd::Won, false, FBattleEventOptions{});
	Won.Run();
	CHECK(Won.State.ProgramResult == BattleModeResult::Won);
	CHECK(Won.Context->Ending.Pose == EBattleEndPose::Victory);

	FEndedBattle Escaped(BattleEnd::Escaped, false, FBattleEventOptions{});
	Escaped.Run();
	CHECK(Escaped.State.ProgramResult == BattleModeResult::Escaped);
	CHECK(Escaped.Context->Ending.Pose == EBattleEndPose::Escape);

	FEndedBattle Lost(BattleEnd::Lost, false, FBattleEventOptions{});
	Lost.Run();
	CHECK(Lost.State.ProgramResult == BattleModeResult::Lost);
	CHECK(Lost.Context->Ending.Pose == EBattleEndPose::None);
	CHECK(Lost.Context->End == BattleEnd::Lost);
}

TEST_CASE("An event battle lost as its story wants goes on as won, without a pose", "[Battle]")
{
	FEndedBattle Battle(BattleEnd::Lost, true, FBattleEventOptions{});
	Battle.Context->Event.Requests.bDefeatScripted = true;
	Battle.Context->Event.Requests.bEndedBattle = true;
	Battle.Run();
	CHECK(Battle.State.ProgramResult == BattleModeResult::ScriptedDefeat);
	CHECK(Battle.Context->End == BattleEnd::Won);
	CHECK(Battle.Context->Ending.Pose == EBattleEndPose::None);
	CHECK(Battle.Context->EventVariables[0] == 0xFF);
}

TEST_CASE("An event battle lost without a scripted defeat ends as lost unless the original's sorting is asked for", "[Battle]")
{
	FEndedBattle Fixed(BattleEnd::Lost, true, FBattleEventOptions{});
	Fixed.Run();
	CHECK(Fixed.State.ProgramResult == BattleModeResult::Lost);
	CHECK(Fixed.Context->EventVariables[0] == 0);

	FEndedBattle Original(BattleEnd::Lost, true, FBattleEventOptions::Original());
	Original.Run();
	CHECK(Original.State.ProgramResult == 0x7E);
	CHECK(Original.Context->Ending.Pose == EBattleEndPose::None);
	CHECK(Original.Context->EventVariables[0] == 0);
}

TEST_CASE("A battle's outcome carries its result and what the event asked of the programs after it", "[Battle]")
{
	FEndedBattle Battle(BattleEnd::Escaped, true, FBattleEventOptions{});
	FBattleEventRequests& Requests = Battle.Context->Event.Requests;
	Requests.Chain = FBattleChain{0x12, 3};
	Requests.Movie = FBattleMovieRequest{0x81, 40, 1, 500};
	Requests.bFieldStartsFresh = true;
	Requests.FieldReturn = 1;
	Battle.Run();
	const FBattleOutcome Outcome = TakeBattleOutcome(*Battle.Context);
	CHECK(Outcome.Result == BattleEnd::Escaped);
	REQUIRE(Outcome.Chain);
	CHECK(Outcome.Chain->Formation == 0x12);
	CHECK(Outcome.Chain->Kind == 3);
	REQUIRE(Outcome.Movie);
	CHECK(Outcome.Movie->Number == 40);
	CHECK(Outcome.Movie->EndFrame == 500);
	CHECK(Outcome.bFieldStartsFresh);
	CHECK(Outcome.FieldReturn == std::optional<uint8>{1});
}
