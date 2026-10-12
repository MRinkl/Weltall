#include "Support/GameDiscs.h"
#include "Support/PlayedGame.h"
#include "Support/PortStory.h"
#include "Support/StorySweepExplore.h"
#include "Support/StorySweepRun.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace Weltall;
using namespace Weltall::Testing;

namespace
{
	/** The game on the checkout's discs, played as a player's build plays it with no talk or touch started outright. */
	struct FPlayed
	{
		FPlayedGame Game;
		FStorySweepOptions Options;

		explicit FPlayed(FPlayedGame&& InGame) : Game(std::move(InGame))
		{
			Options.bForcesTriggers = false;
		}
	};

	FStorySweepStretch Begin(FStorySweepRun& Run, const FPlayed& Played, EStorySweepTrigger Kind, int32 Number)
	{
		REQUIRE(Run.Ask({Kind, Number}));
		return Run.Play(Played.Options.Triggers);
	}

	/** The frames a stretch is given where only its beginning is looked at: the scene it starts is long. */
	constexpr int32 BeginningFrames = 600;
}

TEST_CASE("A box that asks to be faced is talked to from beside it", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	// The cell of the sand cruiser: Citan lies on his bunk, a box that talks only to a leader facing it. The bunk
	// is floor too, and at the box's own middle nobody faces it.
	constexpr int32 Cell = 56;
	constexpr uint16 Captured = 42;
	constexpr int32 Citan = 4;
	REQUIRE_FALSE(Run.Enter(Cell, Captured));
	REQUIRE(Run.Play(Played.Options.Scenes).End == EStorySweepEnd::Control);
	const FStorySweepStretch Talk = Begin(Run, Played, EStorySweepTrigger::Talk, Citan);
	INFO(Talk.Path << " | " << Talk.Detail);
	CHECK(Talk.Opening == EStorySweepOpening::Answered);
	CHECK(Talk.End == EStorySweepEnd::Control);
	CHECK(Talk.Map == Cell);
}

TEST_CASE("A box with floor by a corner of its rim only is talked to from there", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	Played.Options.StretchFrames = BeginningFrames;
	// The music box of Citan's house, on the evening the story's first day ends with: a box on the round table, whose
	// reach the floor meets only past one of its corners, a few units of it; and its script ends at once unless the
	// leader faces the shelf behind it. The door is talked to first and leads out; come back in, the box is next.
	constexpr int32 Workshop = 21;
	constexpr uint16 Evening = 12;
	std::vector<FGameState> Roots = StorySweepRoots(Played.Game, {});
	REQUIRE(Roots.size() == 1);
	Roots[0].SetPlace(Workshop);
	Roots[0].SetSavedEventVariable(0, Evening);
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	std::vector<std::pair<std::string, FStorySweepStretch>> Told;
	FStorySweepExplorer Explorer(Run, Played.Options, [&](int32, uint16, const std::string& Start, const FStorySweepRun&, const FStorySweepStretch& Stretch)
		{
			Told.emplace_back(Start, Stretch);
		});
	Explorer.Explore(Roots[0], 1);
	const auto Wound = std::find_if(Told.begin(), Told.end(), [](const auto& Each) { return Each.first.rfind("talk 15", 0) == 0; });
	REQUIRE(Wound != Told.end());
	INFO(Wound->first << ": " << Wound->second.Path << " | " << Wound->second.Detail);
	CHECK(Wound->first == "talk 15");
	// Its scene is the day's last, and long: the scripts still have the party when the stretch's frames run out.
	CHECK(Wound->second.End == EStorySweepEnd::Long);
}

TEST_CASE("A zone whose middle lies off the floor is walked into the way the leader's walk comes to it", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	Played.Options.StretchFrames = BeginningFrames;
	// Bledavik's upper streets, come onto from the house south of the inn. The inn's door is a zone past the head
	// of its steps: only the doorstep's edge lies in it, which neither the zone's middle nor the straight way to it
	// from that house meets.
	constexpr int32 Streets = 102;
	constexpr int32 Inn = 105;
	constexpr uint16 LookingForAWayIn = 62;
	constexpr uint16 FromTheHouseToTheSouth = 2;
	constexpr int32 InnsDoor = 5;
	std::vector<FGameState> Roots = StorySweepRoots(Played.Game, {});
	REQUIRE(Roots.size() == 1);
	Roots[0].SetPlace(Streets);
	Roots[0].SetFieldEntrance(FromTheHouseToTheSouth);
	Roots[0].SetSavedEventVariable(0, LookingForAWayIn);
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	REQUIRE_FALSE(Run.Resume(Roots[0]));
	REQUIRE(Run.Play(Played.Options.Scenes).End == EStorySweepEnd::Control);
	const FStorySweepStretch Entered = Begin(Run, Played, EStorySweepTrigger::Area, InnsDoor);
	INFO(Entered.Path << " | " << Entered.Detail);
	CHECK(Entered.Opening == EStorySweepOpening::Answered);
	CHECK(Entered.Path.rfind(std::to_string(Streets) + " > " + std::to_string(Inn), 0) == 0);
}

TEST_CASE("A screen that goes on by itself and takes no confirm is left by another button", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	// The chart on the Yggdrasil's bridge: it names the place its cursor is on over and over, a window opening and
	// closing with no instruction new, and only the cancel button leaves it.
	constexpr int32 Bridge = 290;
	constexpr int32 Chart = 305;
	constexpr uint16 AboardTheYggdrasil = 60;
	constexpr int32 ChartTable = 23;
	REQUIRE_FALSE(Run.Enter(Bridge, AboardTheYggdrasil));
	REQUIRE(Run.Play(Played.Options.Scenes).End == EStorySweepEnd::Control);
	const FStorySweepStretch Looked = Begin(Run, Played, EStorySweepTrigger::Talk, ChartTable);
	INFO(Looked.Path << " | " << Looked.Detail);
	CHECK(Looked.Opening == EStorySweepOpening::Answered);
	CHECK(Looked.End == EStorySweepEnd::Control);
	CHECK(Looked.Map == Bridge);
	CHECK(Looked.Path == std::to_string(Bridge) + " > " + std::to_string(Chart) + " > " + std::to_string(Bridge));
}

TEST_CASE("A talk that said something new is begun again", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	// The cell again: Citan's first words are of the gear, and only talked to a second time does he ask whether to
	// rest, which is the story's way on.
	constexpr int32 Cell = 56;
	constexpr uint16 Captured = 42;
	std::vector<FGameState> Roots = StorySweepRoots(Played.Game, {});
	REQUIRE(Roots.size() == 1);
	Roots[0].SetPlace(Cell);
	Roots[0].SetSavedEventVariable(0, Captured);
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	std::vector<std::pair<std::string, FStorySweepStretch>> Told;
	FStorySweepExplorer Explorer(Run, Played.Options, [&](int32, uint16, const std::string& Start, const FStorySweepRun&, const FStorySweepStretch& Stretch)
		{
			Told.emplace_back(Start, Stretch);
		});
	Explorer.Explore(Roots[0], 1);
	const auto Line = [&](const std::string& Start) -> const FStorySweepStretch*
	{
		const auto Found = std::find_if(Told.begin(), Told.end(), [&](const auto& Each) { return Each.first == Start; });
		return Found != Told.end() ? &Found->second : nullptr;
	};
	const FStorySweepStretch* First = Line("talk 4");
	REQUIRE(First != nullptr);
	CHECK(First->End == EStorySweepEnd::Control);
	CHECK(First->Map == Cell);
	const FStorySweepStretch* Second = Line("talk 4 again");
	REQUIRE(Second != nullptr);
	INFO(Second->Path << " | " << Second->Detail);
	// The night's scenes begin: they leave the cell.
	CHECK(Second->Path.rfind(std::to_string(Cell) + " > ", 0) == 0);
}

TEST_CASE("A zone that answers whoever stays in it is stood in before it is walked on through", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	Played.Options.StretchFrames = BeginningFrames;
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	// Bledavik's main street, a strip with a band across it at each gate: stood on one, the party is shown the name
	// of where it leads and then taken there; walked across, the name is gone again and the party is still on the street.
	constexpr int32 MainStreet = 101;
	constexpr int32 SouthBledavik = 102;
	constexpr uint16 BeforeTheTournament = 64;
	constexpr int32 SouthGate = 1;
	REQUIRE_FALSE(Run.Enter(MainStreet, BeforeTheTournament));
	REQUIRE(Run.Play(Played.Options.Scenes).End == EStorySweepEnd::Control);
	const FStorySweepStretch Entered = Begin(Run, Played, EStorySweepTrigger::Area, SouthGate);
	INFO(Entered.Path << " | " << Entered.Detail);
	CHECK(Entered.Opening == EStorySweepOpening::Answered);
	CHECK(Entered.Path.rfind(std::to_string(MainStreet) + " > " + std::to_string(SouthBledavik), 0) == 0);
}

TEST_CASE("A gate that asks for the confirm button held as its name closes is given it", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	Played.Options.StretchFrames = BeginningFrames;
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	// The paths around Nisan's lake. A mark on the path shows the name of where it leads while the party stands on
	// it; the confirm button closes the name, and the mark's script goes on only if the button is still held then.
	constexpr int32 LakePaths = 94;
	constexpr int32 Nisan = 93;
	constexpr uint16 ArrivedInNisan = 81;
	constexpr int32 TownsMark = 4;
	REQUIRE_FALSE(Run.Enter(LakePaths, ArrivedInNisan));
	REQUIRE(Run.Play(Played.Options.Scenes).End == EStorySweepEnd::Control);
	const FStorySweepStretch Entered = Begin(Run, Played, EStorySweepTrigger::Area, TownsMark);
	INFO(Entered.Path << " | " << Entered.Detail);
	CHECK(Entered.Opening == EStorySweepOpening::Answered);
	CHECK(Entered.Path.rfind(std::to_string(LakePaths) + " > " + std::to_string(Nisan), 0) == 0);
}

TEST_CASE("A talk someone walking on bars the way to is asked again once they stand still", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	Played.Options.StretchFrames = BeginningFrames;
	// The corridor of Nisan's cathedral at the town's sixteenth step: Margie, whose body fills it, walks down it ahead
	// of the party to its far door and then follows the leader. The door is reached only behind her, some seconds on;
	// talked to, it leads to the picture's room, where the story goes on to its next value.
	constexpr int32 Corridor = 100;
	constexpr int32 PicturesRoom = 98;
	constexpr uint16 CityOfPeace = 81;
	constexpr uint32 NisansSteps = 0x102;
	constexpr uint16 FollowingMargie = 16;
	constexpr int32 FarDoor = 15;
	Played.Options.Variables.push_back({NisansSteps, '=', FollowingMargie});
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	REQUIRE_FALSE(Run.Enter(Corridor, CityOfPeace));
	REQUIRE(Run.Play(Played.Options.Scenes).End == EStorySweepEnd::Control);
	const FStorySweepStretch Talk = Begin(Run, Played, EStorySweepTrigger::Talk, FarDoor);
	INFO(Talk.Path << " | " << Talk.Detail << " | " << Talk.Barred);
	CHECK(Talk.Opening == EStorySweepOpening::Answered);
	CHECK(Talk.Path.rfind(std::to_string(Corridor) + " > " + std::to_string(PicturesRoom), 0) == 0);
}
