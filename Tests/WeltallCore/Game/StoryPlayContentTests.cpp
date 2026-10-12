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
	/** A stretch as an exploration told of it. */
	struct FToldStretch
	{
		int32 Map = 0;
		uint16 Story = 0;
		std::string Start;
		FStorySweepStretch Stretch;
	};

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

	const FToldStretch* Find(const std::vector<FToldStretch>& Told, const std::string& Start)
	{
		const auto Found = std::find_if(Told.begin(), Told.end(), [&](const FToldStretch& Line) { return Line.Start == Start; });
		return Found != Told.end() ? &*Found : nullptr;
	}
}

TEST_CASE("A new game plays by itself from its opening to the player's control in Fei's room", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	const std::vector<FGameState> Roots = StorySweepRoots(Played.Game, {});
	REQUIRE(Roots.size() == 1);
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	REQUIRE_FALSE(Run.Resume(Roots[0]));

	// The prologue over Lahan, the village and its first battle (fought by a random pad), then Fei wakes in his room.
	const FStorySweepStretch Opening = Run.Play(Played.Options.Scenes);
	INFO(Opening.Path << " | " << Opening.Detail);
	CHECK(Opening.End == EStorySweepEnd::Control);
	CHECK(Opening.Path == "4 > 2 > battle 1 > 14");
	CHECK(Opening.Map == 14);
	CHECK(Opening.Story == 6);
	CHECK(Opening.Unreached.empty());

	// The room's ways: its zones are among what a player can do there, and a walk into the first leads downstairs.
	const std::vector<FStorySweepTrigger> Triggers = Run.Triggers();
	const auto Stairs = std::find_if(Triggers.begin(), Triggers.end(), [](const FStorySweepTrigger& Trigger) { return Trigger.Kind == EStorySweepTrigger::Area; });
	REQUIRE(Stairs != Triggers.end());
	CHECK(Stairs->Name() == "area 0");
	REQUIRE(Run.Ask(*Stairs));
	const FStorySweepStretch Down = Run.Play(Played.Options.Triggers);
	INFO(Down.Path << " | " << Down.Detail);
	CHECK(Down.Opening == EStorySweepOpening::Answered);
	CHECK(Down.End == EStorySweepEnd::Control);
	CHECK(Down.Path == "14 > 13");
	CHECK(Down.Story == 6);
}

TEST_CASE("A place is explored by its talks and its ways out as a player finds them", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	// Lee's house downstairs on the morning the story begins with (map 13 at 6), from a new game's state.
	std::vector<FGameState> Roots = StorySweepRoots(Played.Game, {});
	REQUIRE(Roots.size() == 1);
	Roots[0].SetPlace(13);
	Roots[0].SetSavedEventVariable(0, 6);
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	std::vector<FToldStretch> Told;
	FStorySweepExplorer Explorer(Run, Played.Options, [&](int32 Map, uint16 Story, const std::string& Start, const FStorySweepRun&, const FStorySweepStretch& Stretch)
		{
			Told.push_back({Map, Story, Start, Stretch});
		});
	Explorer.Explore(Roots[0], 1);

	// The place's own scenes first, then its talks before its zones; nothing stops, and nothing keeps the party.
	REQUIRE(Told.size() > 4);
	CHECK(Told[0].Start == "scene");
	for (const FToldStretch& Line : Told)
	{
		INFO(Line.Start << ": " << Line.Stretch.Path << " | " << Line.Stretch.Detail);
		CHECK(Line.Map == 13);
		CHECK(Line.Story == 6);
		CHECK(Line.Stretch.End == EStorySweepEnd::Control);
	}
	const auto FirstArea = std::find_if(Told.begin(), Told.end(), [](const FToldStretch& Line) { return Line.Start.rfind("area", 0) == 0; });
	const auto LastTalk = std::find_if(Told.rbegin(), Told.rend(), [](const FToldStretch& Line) { return Line.Start.rfind("talk", 0) == 0; });
	REQUIRE(FirstArea != Told.end());
	REQUIRE(LastTalk != Told.rend());
	CHECK(&*LastTalk < &*FirstArea);

	// A talk a player begins is answered by the game's own check and gives the party back where it was.
	const FToldStretch* Talk = Find(Told, "talk 12");
	REQUIRE(Talk != nullptr);
	CHECK(Talk->Stretch.Opening == EStorySweepOpening::Answered);
	CHECK(Talk->Stretch.Map == 13);
	// The stairs' zone leads up to Fei's room; the zone by the table plays the morning's scene, which moves the story on.
	const FToldStretch* Stairs = Find(Told, "area 0");
	REQUIRE(Stairs != nullptr);
	CHECK(Stairs->Stretch.Path == "13 > 14");
	const FToldStretch* Scene = Find(Told, "area 1");
	REQUIRE(Scene != nullptr);
	CHECK(Scene->Stretch.Map == 13);
	CHECK(Scene->Stretch.Story == 7);
}

TEST_CASE("The gears' ambush in the desert plays through its gears' changes to its battle", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	// The fight is not the test's: a moment of it ends the stretch.
	Played.Options.BattleFrames = 60;
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	// The desert on the way to Aveh's border, the story where the Gebler unit lies in wait there.
	constexpr int32 Desert = 174;
	constexpr uint16 BeforeTheAmbush = 87;
	REQUIRE_FALSE(Run.Enter(Desert, BeforeTheAmbush));
	const FStorySweepStretch Arrival = Run.Play(Played.Options.Scenes);
	INFO(Arrival.Path << " | " << Arrival.Detail);
	REQUIRE(Arrival.End == EStorySweepEnd::Control);

	// A walk into the map's middle begins it. The unit's gears come one after another on one actor, each freed a
	// tick before the next is loaded, while the last particles of its thrusters are still due on its joints.
	REQUIRE(Run.Ask({EStorySweepTrigger::Area, 2}));
	const FStorySweepStretch Ambush = Run.Play(Played.Options.Triggers);
	INFO(Ambush.Path << " | " << Ambush.Detail);
	CHECK(Ambush.End == EStorySweepEnd::BattleRuns);
	CHECK(Ambush.Path == "174 > battle 14");
	CHECK(Ambush.Unreached.empty());
}

TEST_CASE("The fights at Aveh's border play on while a model still moves by an attack's freed file", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	// The fights are won, as a player wins them: what follows them is the test's.
	Played.Options.bStrongParty = true;
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	constexpr int32 Desert = 174;
	constexpr uint16 BeforeTheAmbush = 87;
	REQUIRE_FALSE(Run.Enter(Desert, BeforeTheAmbush));
	REQUIRE(Run.Play(Played.Options.Scenes).End == EStorySweepEnd::Control);

	// The Gebler unit, then Vierge. As an action of that fight ends, the file its sequences and animations came
	// from is given back while a bone still plays the last of one of them: the bone reads on from the block.
	REQUIRE(Run.Ask({EStorySweepTrigger::Area, 2}));
	const FStorySweepStretch Fought = Run.Play(Played.Options.Triggers);
	INFO(Fought.Path << " | " << Fought.Detail);
	CHECK(Fought.End == EStorySweepEnd::Control);
	CHECK(Fought.Path == "174 > battle 14 > battle 14 > 174 > 176 > 174 > battle 15 > 177 > 172");
	CHECK(Fought.Story == 90);
	CHECK(Fought.Unreached.empty());
}
