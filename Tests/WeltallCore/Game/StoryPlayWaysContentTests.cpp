#include "Support/GameDiscs.h"
#include "Support/PlayedGame.h"
#include "Support/PortStory.h"
#include "Support/StorySweepExplore.h"
#include "Support/StorySweepRun.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
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

	/** A stretch as an exploration told of it. */
	struct FToldStretch
	{
		int32 Map = 0;
		std::string Start;
		FStorySweepStretch Stretch;
	};

	FStorySweepStretch Begin(FStorySweepRun& Run, const FPlayed& Played, EStorySweepTrigger Kind, int32 Number)
	{
		REQUIRE(Run.Ask({Kind, Number}));
		return Run.Play(Played.Options.Triggers);
	}
}

TEST_CASE("A talk behind a touch the walk has not met is not begun before it", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	// The Thames' survivors late in the story: Hans and Lance stand in the round room, whose mouth an unseen
	// actor's touch fills. Whoever walks in meets it, and its scene sends Lance out to the hall.
	constexpr int32 Shelter = 688;
	constexpr uint16 LateInTheStory = 340;
	constexpr int32 Hans = 14;
	constexpr int32 Lance = 16;
	constexpr int32 RoomsMouth = 18;
	REQUIRE_FALSE(Run.Enter(Shelter, LateInTheStory));
	REQUIRE(Run.Play(Played.Options.Scenes).End == EStorySweepEnd::Control);

	// Stood beside Hans outright, the leader would talk to him with Lance still by his side: a state no walk makes.
	const FStorySweepStretch Early = Begin(Run, Played, EStorySweepTrigger::Talk, Hans);
	INFO(Early.Path << " | " << Early.Detail);
	CHECK(Early.Opening == EStorySweepOpening::Unreached);
	CHECK(Early.End == EStorySweepEnd::Control);

	const FStorySweepStretch Mouth = Begin(Run, Played, EStorySweepTrigger::Touch, RoomsMouth);
	INFO(Mouth.Path << " | " << Mouth.Detail);
	CHECK(Mouth.Opening == EStorySweepOpening::Answered);
	REQUIRE(Mouth.End == EStorySweepEnd::Control);

	// The room is open now, and Lance, talked to where the scene left him, gives the party back.
	const FStorySweepStretch Talk = Begin(Run, Played, EStorySweepTrigger::Talk, Hans);
	INFO(Talk.Path << " | " << Talk.Detail);
	CHECK(Talk.Opening == EStorySweepOpening::Answered);
	REQUIRE(Talk.End == EStorySweepEnd::Control);
	const FStorySweepStretch Son = Begin(Run, Played, EStorySweepTrigger::Talk, Lance);
	INFO(Son.Path << " | " << Son.Detail);
	CHECK(Son.Opening == EStorySweepOpening::Answered);
	CHECK(Son.End == EStorySweepEnd::Control);
}

TEST_CASE("A scene that waits for a button other than the confirm is given it", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	// The mirror that shows a member's reflection: its buttons each play a gesture, and Down alone leaves it.
	constexpr int32 MirrorRoom = 699;
	constexpr uint16 LateInTheStory = 340;
	constexpr int32 Mirror = 13;
	REQUIRE_FALSE(Run.Enter(MirrorRoom, LateInTheStory));
	REQUIRE(Run.Play(Played.Options.Scenes).End == EStorySweepEnd::Control);
	const FStorySweepStretch Viewed = Begin(Run, Played, EStorySweepTrigger::Talk, Mirror);
	INFO(Viewed.Path << " | " << Viewed.Detail);
	CHECK(Viewed.Opening == EStorySweepOpening::Answered);
	CHECK(Viewed.End == EStorySweepEnd::Control);
	CHECK(Viewed.Map == MirrorRoom);
}

TEST_CASE("A place whose entry a stay changed is come to again as it first was", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	// The floor below the lift of the buried facility: its map's start says the lift's cabin stands there.
	constexpr int32 Below = 367;
	constexpr int32 Cabin = 386;
	constexpr int32 Hall = 384;
	constexpr uint16 InTheFacility = 166;
	std::vector<FGameState> Roots = StorySweepRoots(Played.Game, {});
	REQUIRE(Roots.size() == 1);
	Roots[0].SetPlace(Below);
	Roots[0].SetSavedEventVariable(0, InTheFacility);
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	std::vector<FToldStretch> Told;
	FStorySweepExplorer Explorer(Run, Played.Options, [&](int32 Map, uint16, const std::string& Start, const FStorySweepRun&, const FStorySweepStretch& Stretch)
		{
			Told.push_back({Map, Start, Stretch});
		});
	Explorer.Explore(Roots[0], 3);

	// The cabin rides up as the party steps in. Entered anew in the state the ride left, from below with the cabin
	// above, it would ride nowhere and never give the party back: no player's state, so the lift is taken again.
	const auto Line = [&](int32 Map, const std::string& Start) -> const FToldStretch*
	{
		const auto Found = std::find_if(Told.begin(), Told.end(), [&](const FToldStretch& Each) { return Each.Map == Map && Each.Start == Start; });
		return Found != Told.end() ? &*Found : nullptr;
	};
	const FToldStretch* Anew = Line(Cabin, "re-entry");
	REQUIRE(Anew != nullptr);
	CHECK(Anew->Stretch.End == EStorySweepEnd::Held);
	const FToldStretch* Ridden = Line(Cabin, "arrival");
	REQUIRE(Ridden != nullptr);
	INFO(Ridden->Stretch.Path << " | " << Ridden->Stretch.Detail);
	CHECK(Ridden->Stretch.End == EStorySweepEnd::Control);
	CHECK(Ridden->Stretch.Map == Cabin);
	// And its door then opens on the hall above (where a new game's Fei meets more than he can fight).
	const std::string ToTheHall = std::to_string(Cabin) + " > " + std::to_string(Hall);
	CHECK(std::any_of(Told.begin(), Told.end(), [&](const FToldStretch& Each) { return Each.Map == Cabin && Each.Stretch.Path.rfind(ToTheHall, 0) == 0; }));
}

TEST_CASE("A list of choices that comes back to itself is left by its last entry", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	// The desk of Nortune's arena: its clerk explains the rules through lists inside lists, each ending with the way back.
	constexpr int32 ArenaDesk = 221;
	constexpr uint16 InNortune = 111;
	constexpr int32 Clerk = 6;
	REQUIRE_FALSE(Run.Enter(ArenaDesk, InNortune));
	REQUIRE(Run.Play(Played.Options.Scenes).End == EStorySweepEnd::Control);
	const FStorySweepStretch Asked = Begin(Run, Played, EStorySweepTrigger::Talk, Clerk);
	INFO(Asked.Path << " | " << Asked.Detail);
	CHECK(Asked.Opening == EStorySweepOpening::Answered);
	CHECK(Asked.End == EStorySweepEnd::Control);
	CHECK(Asked.Map == ArenaDesk);
}

TEST_CASE("The world map is come onto by a way out's edge and crossed to the places its land reaches", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	// Lahan on the morning the story begins with. Its ways out take the party only at the screen's edge.
	constexpr int32 Lahan = 1;
	constexpr int32 MountainPath = 15;
	constexpr int32 WorldMap = 0x400;
	constexpr uint16 Morning = 7;
	constexpr int32 SouthernWayOut = 7;
	REQUIRE_FALSE(Run.Enter(Lahan, Morning));
	REQUIRE(Run.Play(Played.Options.Scenes).End == EStorySweepEnd::Control);
	const FStorySweepStretch Left = Begin(Run, Played, EStorySweepTrigger::Area, SouthernWayOut);
	INFO(Left.Path << " | " << Left.Detail);
	REQUIRE(Left.End == EStorySweepEnd::WorldMap);
	CHECK(Left.Map == WorldMap);

	// From there the party walks to the village and to the mountain path, and nowhere else yet: each is entered
	// with the party standing in it, as a game saved there goes on.
	const std::vector<FStorySweepTrigger> Places = Run.Triggers();
	REQUIRE(Places.size() == 2);
	CHECK(Places[0].Kind == EStorySweepTrigger::Place);
	CHECK(Places[0].Name() == "place 0.0");
	const FGameState OnTheWorldMap = Run.State();
	std::vector<int32> Entered;
	for (const FStorySweepTrigger& Place : Places)
	{
		REQUIRE_FALSE(Run.Resume(OnTheWorldMap));
		REQUIRE(Run.Play(Played.Options.Scenes).End == EStorySweepEnd::WorldMap);
		REQUIRE(Run.Ask(Place));
		const FStorySweepStretch Walked = Run.Play(Played.Options.Triggers);
		INFO(Place.Name() << ": " << Walked.Path << " | " << Walked.Detail);
		CHECK(Walked.Opening == EStorySweepOpening::Answered);
		CHECK(Walked.End == EStorySweepEnd::Control);
		Entered.push_back(Walked.Map);
	}
	CHECK(Entered == std::vector<int32>{MountainPath, Lahan});
}

TEST_CASE("A floor no walk reaches is gone onto as a player gets there and the way past it taken", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	Played.Options.bStrongParty = true;
	// The Mountain Path on the morning the story begins, come onto from the world map. Its upper half lies on the same
	// layer as the lower, closed off between them: the way up is a walkway of the layer above, which the floor at its
	// foot hands the walker onto, and off whose top end a walk steps down onto the upper path.
	constexpr int32 MountainPath = 15;
	constexpr int32 AboveThePath = 17;
	constexpr uint16 Morning = 7;
	constexpr uint16 FromTheWorldMap = 1;
	constexpr int32 WayUp = 11;
	std::vector<FGameState> Roots = StorySweepRoots(Played.Game, {});
	REQUIRE(Roots.size() == 1);
	Roots[0].SetPlace(MountainPath);
	Roots[0].SetFieldEntrance(FromTheWorldMap);
	Roots[0].SetSavedEventVariable(0, Morning);
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	REQUIRE_FALSE(Run.Resume(Roots[0]));
	REQUIRE(Run.Play(Played.Options.Scenes).End == EStorySweepEnd::Control);

	// No walk on the lower path gets to the way up.
	const FStorySweepStretch Unwalked = Begin(Run, Played, EStorySweepTrigger::Area, WayUp);
	INFO(Unwalked.Path << " | " << Unwalked.Detail);
	CHECK(Unwalked.Opening == EStorySweepOpening::Unreached);

	const std::vector<FStorySweepTrigger> Triggers = Run.Triggers();
	const auto Named = [&](const std::string& Name)
	{
		return std::any_of(Triggers.begin(), Triggers.end(), [&](const FStorySweepTrigger& Trigger) { return Trigger.Name() == Name; });
	};
	REQUIRE(Named("floor 1.18"));
	CHECK_FALSE(Named("floor 0.1136"));
	const FStorySweepStretch Walkway = Begin(Run, Played, EStorySweepTrigger::Floor, 1 * FloorLayerSpan + 18);
	INFO(Walkway.Path << " | " << Walkway.Detail);
	CHECK(Walkway.Opening == EStorySweepOpening::Answered);
	REQUIRE(Walkway.End == EStorySweepEnd::Control);
	const FStorySweepStretch UpperPath = Begin(Run, Played, EStorySweepTrigger::Floor, 0 * FloorLayerSpan + 1136);
	INFO(UpperPath.Path << " | " << UpperPath.Detail);
	CHECK(UpperPath.Opening == EStorySweepOpening::Answered);
	REQUIRE(UpperPath.End == EStorySweepEnd::Control);

	// From there the way up is walked to, by the crossings found, and leads on up the mountain (a random battle may
	// break into the walk first: then it is taken again).
	FStorySweepStretch Up = Begin(Run, Played, EStorySweepTrigger::Area, WayUp);
	if (Up.Map == MountainPath && Up.Path.find("battle") != std::string::npos)
	{
		Up = Begin(Run, Played, EStorySweepTrigger::Area, WayUp);
	}
	INFO(Up.Path << " | " << Up.Detail);
	CHECK(Up.Opening == EStorySweepOpening::Answered);
	CHECK(Up.End == EStorySweepEnd::Control);
	CHECK(Up.Map == AboveThePath);
}

TEST_CASE("A member the scripts keep standing is not brought along when the leader is stood by a trigger", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	// Map 147 at 90 with Bart's party, Bart and Citan: the map's scripts keep Citan, the party's second, standing, and
	// he does not follow. Stood on the leader at each walk's start, his body would hold it and three talks start none.
	constexpr int32 BartsMap = 147;
	constexpr uint16 WithCitan = 90;
	constexpr std::array<uint8, 3> BartAndCitan = {3, 2, 0xFF};
	constexpr std::array<int32, 3> Crew = {17, 19, 41};
	std::vector<FGameState> Roots = StorySweepRoots(Played.Game, {});
	REQUIRE(Roots.size() == 1);
	Roots[0].SetPlace(BartsMap);
	Roots[0].SetSavedEventVariable(0, WithCitan);
	Roots[0].SetParty(BartAndCitan);
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	REQUIRE_FALSE(Run.Resume(Roots[0]));
	const FStorySweepStretch Scene = Run.Play(Played.Options.Scenes);
	INFO(Scene.Path << " | " << Scene.Detail);
	REQUIRE(Scene.End == EStorySweepEnd::Control);
	for (const int32 Member : Crew)
	{
		const FStorySweepStretch Talk = Begin(Run, Played, EStorySweepTrigger::Talk, Member);
		INFO("talk " << Member << ": " << Talk.Path << " | " << Talk.Detail);
		CHECK(Talk.Opening == EStorySweepOpening::Answered);
		CHECK(Talk.End == EStorySweepEnd::Control);
	}
}

TEST_CASE("A zone another floor stands at the height of is walked into from that floor", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	// The stairs between the castle's galleries (map 157), Bart at their foot. Each of the two landings above the
	// stairs (layer 1, west and east, never joined) has a way on to map 158, a zone tested with its height (zones 3 and
	// 2, 283 units up) over the stairs' own ways down to map 156 (zones 1 and 0): walked into on the stairs, the walk
	// met the stairs' zone first. Once a crossing onto the west landing is found, its zone is walked into from there.
	constexpr int32 Stairs = 157;
	constexpr int32 PastTheLanding = 158;
	constexpr uint16 InTheCastle = 70;
	constexpr std::array<uint8, 3> BartAlone = {3, 0xFF, 0xFF};
	constexpr uint16 AtTheFoot = 0;
	constexpr int32 TheWestLandingsWayOn = 3;
	constexpr int32 Landings = 1;
	std::vector<FGameState> Roots = StorySweepRoots(Played.Game, {});
	REQUIRE(Roots.size() == 1);
	Roots[0].SetPlace(Stairs);
	Roots[0].SetParty(BartAlone);
	Roots[0].SetFieldEntrance(AtTheFoot);
	Roots[0].SetSavedEventVariable(0, InTheCastle);
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	REQUIRE_FALSE(Run.Resume(Roots[0]));
	REQUIRE(Run.Play(Played.Options.Scenes).End == EStorySweepEnd::Control);

	bool bCrossed = false;
	for (const FStorySweepTrigger& Trigger : Run.Triggers())
	{
		if (!bCrossed && Trigger.Kind == EStorySweepTrigger::Floor && Trigger.Number / FloorLayerSpan == Landings)
		{
			const FStorySweepStretch Crossing = Begin(Run, Played, Trigger.Kind, Trigger.Number);
			bCrossed = Crossing.Opening == EStorySweepOpening::Answered && Crossing.Map == Stairs;
		}
	}
	REQUIRE(bCrossed);
	const FStorySweepStretch On = Begin(Run, Played, EStorySweepTrigger::Area, TheWestLandingsWayOn);
	INFO(On.Path << " | " << On.Detail << " | " << On.Barred);
	CHECK(On.Opening == EStorySweepOpening::Answered);
	CHECK(On.Path.rfind(std::to_string(Stairs) + " > " + std::to_string(PastTheLanding), 0) == 0);
}

TEST_CASE("A zone reached by a ladder of the walk's own layer is walked to on that layer", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FPlayed Played(std::move(*Opened));
	Played.Options.bStrongParty = true;
	// Up from Aveh's waterway (map 147), Bart alone at the foot (layer 0). The way on to the castle (zone 0, tested with
	// its height) is at the top of a ladder of layer 1, which the ground walked on that layer leaves out: from the landing
	// it is walked to and climbed, not taken for another floor's.
	constexpr int32 UpFromTheWaterway = 147;
	constexpr int32 BelowTheArena = 133;
	constexpr uint16 InTheWaterway = 70;
	constexpr std::array<uint8, 3> BartAlone = {3, 0xFF, 0xFF};
	constexpr uint16 FromTheWaterway = 1;
	constexpr int32 TheWayOn = 0;
	constexpr int32 Landing = 1;
	std::vector<FGameState> Roots = StorySweepRoots(Played.Game, {});
	REQUIRE(Roots.size() == 1);
	Roots[0].SetPlace(UpFromTheWaterway);
	Roots[0].SetParty(BartAlone);
	Roots[0].SetFieldEntrance(FromTheWaterway);
	Roots[0].SetSavedEventVariable(0, InTheWaterway);
	FStorySweepRun Run(Played.Game, {}, &PortStory(), Played.Options);
	REQUIRE_FALSE(Run.Resume(Roots[0]));
	REQUIRE(Run.Play(Played.Options.Scenes).End == EStorySweepEnd::Control);

	bool bCrossed = false;
	for (const FStorySweepTrigger& Trigger : Run.Triggers())
	{
		if (!bCrossed && Trigger.Kind == EStorySweepTrigger::Floor && Trigger.Number / FloorLayerSpan == Landing)
		{
			const FStorySweepStretch Crossing = Begin(Run, Played, Trigger.Kind, Trigger.Number);
			bCrossed = Crossing.Opening == EStorySweepOpening::Answered && Crossing.Map == UpFromTheWaterway;
		}
	}
	REQUIRE(bCrossed);
	const FStorySweepStretch On = Begin(Run, Played, EStorySweepTrigger::Area, TheWayOn);
	INFO(On.Path << " | " << On.Detail << " | " << On.Barred);
	CHECK(On.Opening == EStorySweepOpening::Answered);
	CHECK(On.Map == BelowTheArena);
}
