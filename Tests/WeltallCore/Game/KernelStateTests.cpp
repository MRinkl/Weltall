#include "Game/GameState.h"
#include "Game/KernelState.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

// The kernel's rules for which program runs next, as its globals hand them over.

TEST_CASE("The kernel starts with the opening's logos then the field, disc 1's movie 16 and disc 2's movie 7", "[Game]")
{
	const FKernelState First = FKernelState::PowerOn(0);
	CHECK(First.Movie.Type == 1);
	CHECK(First.Movie.Number == 16);
	CHECK(First.Movie.NextProgram == EGameProgram::Field);
	CHECK(First.Movie.NoSkip == 0);
	CHECK(First.NextProgram == EGameProgram::DebugMenu);
	CHECK(FKernelState::PowerOn(1).Movie.Number == 7);
	CHECK(First.Disc == 0);
	CHECK(FKernelState::PowerOn(1).Disc == 1);
}

TEST_CASE("A new game sets the windows' colour and blend and the battle's debug at rest", "[Game]")
{
	FKernelState Kernel;
	Kernel.InitNewGame();
	CHECK(Kernel.bBattleDebug);
	CHECK(Kernel.WindowColour == std::array<uint8, 3>{0x88, 0x76, 0x54});
	CHECK(Kernel.BattleWindowBlend == 2);
}

TEST_CASE("After a battle won or escaped the kernel runs its movie or the chained battle or the place's program", "[Game]")
{
	FGameState Game;
	FKernelState Kernel;
	Game.SetPlace(0x123);
	Kernel.EndBattle({FKernelState::BattleWon, true}, Game);
	CHECK(Kernel.NextProgram == EGameProgram::Movie);
	CHECK(Kernel.bBattleDebug);

	Kernel = {};
	Kernel.ChainedBattle = 5;
	Kernel.EndBattle({FKernelState::BattleEscaped, false}, Game);
	CHECK(Kernel.NextProgram == EGameProgram::Battle);
	CHECK_FALSE(Kernel.bBattleDebug);

	Kernel = {};
	Kernel.EndBattle({FKernelState::BattleWon, false}, Game);
	CHECK(Kernel.NextProgram == EGameProgram::Field);
	// The place's gear bits are not part of the map it names.
	Game.SetPlace(0x4123);
	Kernel.EndBattle({FKernelState::BattleResultUnused, false}, Game);
	CHECK(Kernel.NextProgram == EGameProgram::Field);
	Game.SetPlace(0x405);
	Kernel.EndBattle({FKernelState::BattleWon, false}, Game);
	CHECK(Kernel.NextProgram == EGameProgram::WorldMap);
}

TEST_CASE("A lost battle sends the field afresh to the title's map with its words cleared", "[Game]")
{
	FGameState Game;
	Game.SetWorldMapPlace(0x405, 3, 0x800, 7);
	FKernelState Kernel;
	Kernel.FieldTrips = 2;
	Kernel.EndBattle({FKernelState::BattleLost, true}, Game);
	CHECK(Kernel.NextProgram == EGameProgram::Field);
	CHECK(Kernel.FieldTrips == 0);
	CHECK(Game.Place() == FKernelState::TitleMap);
	CHECK(Game.CameraYaw() == 0);
	CHECK(Game.Scene() == 0);
	CHECK(Kernel.bBattleDebug);
}

TEST_CASE("A battle with any other result leaves the next program as the kernel set it", "[Game]")
{
	FGameState Game;
	FKernelState Kernel;
	Kernel.EndBattle({0, false}, Game);
	CHECK(Kernel.NextProgram == EGameProgram::DebugMenu);
}

TEST_CASE("The kernel's formation table gives each of its 16 records", "[Game]")
{
	FKernelState Kernel;
	for (std::size_t Byte = 0; Byte < Kernel.Formations.size(); ++Byte)
	{
		Kernel.Formations[Byte] = static_cast<uint8>(Byte);
	}
	const auto Record = Kernel.Formation(3);
	REQUIRE(Record);
	CHECK((*Record)[0] == 0x60);
	CHECK((*Record)[31] == 0x7F);
	CHECK_FALSE(Kernel.Formation(16));
	CHECK_FALSE(Kernel.Formation(-1));
}
