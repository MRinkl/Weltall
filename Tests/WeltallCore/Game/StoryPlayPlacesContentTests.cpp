#include "Support/GameDiscs.h"
#include "Support/PlayedGame.h"
#include "Support/PortStory.h"
#include "Support/StorySweepExplore.h"
#include "Support/StorySweepRun.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

using namespace Weltall;
using namespace Weltall::Testing;

namespace
{
	/** A word of the game's state, where the world map keeps the ship. */
	void PutWord(FGameState& State, std::size_t Offset, uint16 Value)
	{
		std::memcpy(State.Block().data() + Offset, &Value, sizeof(Value));
	}

	/** A stretch as an exploration told of it. */
	struct FToldStretch
	{
		uint16 Story = 0;
		std::string Start;
		FStorySweepStretch Stretch;
	};
}

TEST_CASE("The ship's own voyage is no travel of the player's and is waited out", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FStorySweepOptions Options;
	Options.bForcesTriggers = false;
	// The voyage's own frames, and the first of the scenes it ends in.
	Options.StretchFrames = 3000;
	// The Yggdrasil's bridge after the Stalactite Cave: Sigurd has the ship sail on to Bart's hideout, which it does by
	// itself over the world map, the pad steering nothing, and docks there.
	constexpr int32 Bridge = 290;
	constexpr int32 Hideout = 80;
	constexpr uint16 AboardForTheFirstTime = 54;
	constexpr std::array<uint8, 3> FeiAndCitan = {0, 2, 0xFF};
	constexpr int32 Sigurd = 12;
	// The ship as the story has left it by then: where it stands (x, y, z, heading), and that it is the sand ship,
	// the party aboard, the world map to start at it.
	constexpr std::size_t ShipWords = 0x182C;
	constexpr std::array<uint16, 5> SandShipAboard = {22016, 0, 12800, 0, 0x6001};
	std::vector<FGameState> Roots = StorySweepRoots(*Opened, {});
	REQUIRE(Roots.size() == 1);
	Roots[0].SetPlace(Bridge);
	Roots[0].SetParty(FeiAndCitan);
	Roots[0].SetSavedEventVariable(0, AboardForTheFirstTime);
	for (std::size_t Word = 0; Word < SandShipAboard.size(); ++Word)
	{
		PutWord(Roots[0], ShipWords + Word * sizeof(uint16), SandShipAboard[Word]);
	}
	FStorySweepRun Run(*Opened, {}, &PortStory(), Options);
	REQUIRE_FALSE(Run.Resume(Roots[0]));
	REQUIRE(Run.Play(Options.Scenes).End == EStorySweepEnd::Control);
	REQUIRE(Run.Ask({EStorySweepTrigger::Talk, Sigurd}));
	const FStorySweepStretch Sailed = Run.Play(Options.Triggers);
	INFO(Sailed.Path << " | " << Sailed.Detail);
	CHECK(Sailed.Opening == EStorySweepOpening::Answered);
	CHECK(Sailed.End != EStorySweepEnd::WorldMap);
	CHECK(Sailed.Path.rfind(std::to_string(Bridge) + " > voyage > " + std::to_string(Hideout), 0) == 0);
}

TEST_CASE("A place come to with a flag its scripts test changed is explored again", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FStorySweepOptions Options;
	Options.bForcesTriggers = false;
	// Dazil's sand buggy rental. Its keeper has no buggy until the party has asked about the desert elsewhere in
	// town and crossed the square again, which other maps' scripts note in a saved variable his own tests.
	constexpr int32 Rental = 36;
	constexpr uint16 InDazil = 38;
	constexpr uint16 BuggyRented = 39;
	constexpr std::array<uint8, 3> FeiAndCitan = {0, 2, 0xFF};
	constexpr int32 DazilsNotes = 0xE4 / 2;
	constexpr uint16 Arrived = 0x1;
	constexpr uint16 AskedAndABuggyBack = 0x1 | 0x40 | 0x400 | 0x800;
	constexpr int32 Keeper = 13;
	std::vector<FGameState> Roots = StorySweepRoots(*Opened, {});
	REQUIRE(Roots.size() == 1);
	Roots[0].SetPlace(Rental);
	Roots[0].SetParty(FeiAndCitan);
	Roots[0].SetSavedEventVariable(0, InDazil);
	Roots[0].SetSavedEventVariable(DazilsNotes, Arrived);
	FGameState Later = Roots[0];
	Later.SetSavedEventVariable(DazilsNotes, AskedAndABuggyBack);

	FStorySweepRun Run(*Opened, {}, &PortStory(), Options);
	std::vector<FToldStretch> Told;
	FStorySweepExplorer Explorer(Run, Options, [&](int32, uint16 Story, const std::string& Start, const FStorySweepRun&, const FStorySweepStretch& Stretch)
		{
			Told.push_back({Story, Start, Stretch});
		});
	const std::string TalkToKeeper = "talk " + std::to_string(Keeper);
	const auto Rented = [&]()
	{
		return std::any_of(Told.begin(), Told.end(), [&](const FToldStretch& Each) { return Each.Start.rfind(TalkToKeeper, 0) == 0 && Each.Stretch.Story == BuggyRented; });
	};
	Explorer.Explore(Roots[0], 1);
	REQUIRE(std::any_of(Told.begin(), Told.end(), [&](const FToldStretch& Each) { return Each.Start == TalkToKeeper; }));
	CHECK_FALSE(Rented());

	// The same map at the same value of the progress, but not the same place: the keeper has a buggy now.
	Explorer.Explore(Later, 1);
	CHECK(Rented());
}

TEST_CASE("A state kept aboard the ship goes on where the ship sails", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FStorySweepOptions Options;
	Options.bForcesTriggers = false;
	// The Yggdrasil in the desert off Bart's hideout, on the way to Aveh: Sigurd launches it when asked, and the
	// party sails the sand. The game writes where the ship is only as it is left for its bridge again.
	constexpr int32 Bridge = 290;
	constexpr uint16 OnTheWayToAveh = 60;
	constexpr std::array<uint8, 3> FeiBartAndCitan = {0, 3, 2};
	constexpr int32 Sigurd = 12;
	constexpr int32 ShipsNotes = 0x1C8 / 2;
	constexpr uint16 MayLaunch = 0x4000;
	constexpr std::size_t ShipWords = 0x182C;
	constexpr std::array<uint16, 5> SandShipOffTheHideout = {18770, static_cast<uint16>(-120), 11145, 512, 0x6001};
	std::vector<FGameState> Roots = StorySweepRoots(*Opened, {});
	REQUIRE(Roots.size() == 1);
	Roots[0].SetPlace(Bridge);
	Roots[0].SetParty(FeiBartAndCitan);
	Roots[0].SetSavedEventVariable(0, OnTheWayToAveh);
	Roots[0].SetSavedEventVariable(ShipsNotes, MayLaunch);
	for (std::size_t Word = 0; Word < SandShipOffTheHideout.size(); ++Word)
	{
		PutWord(Roots[0], ShipWords + Word * sizeof(uint16), SandShipOffTheHideout[Word]);
	}
	FStorySweepRun Run(*Opened, {}, &PortStory(), Options);
	REQUIRE_FALSE(Run.Resume(Roots[0]));
	REQUIRE(Run.Play(Options.Scenes).End == EStorySweepEnd::Control);
	REQUIRE(Run.Ask({EStorySweepTrigger::Talk, Sigurd}));
	const FStorySweepStretch Launched = Run.Play(Options.Triggers);
	INFO(Launched.Path << " | " << Launched.Detail);
	REQUIRE(Launched.End == EStorySweepEnd::WorldMap);

	const auto Names = [](const std::vector<FStorySweepTrigger>& Places)
	{
		std::string All;
		for (const FStorySweepTrigger& Place : Places)
		{
			All += Place.Name() + "; ";
		}
		return All;
	};
	// Where its sand reaches: the Road to Nisan and the hideout's dock.
	const std::string Sailed = Names(Run.Triggers());
	CHECK(Sailed == "place 2.0; place 2.1; ");
	const FGameState Aboard = Run.State();
	REQUIRE_FALSE(Run.Resume(Aboard));
	REQUIRE(Run.Play(Options.Scenes).End == EStorySweepEnd::WorldMap);
	CHECK(Names(Run.Triggers()) == Sailed);
}

TEST_CASE("A question asked again in a later talk is given its other answer", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FStorySweepOptions Options;
	Options.bForcesTriggers = false;
	// The night before the tournament, in the room at Bledavik's inn: Citan asks "Going out again?", and only "Get
	// some rest", the second answer, ends the day. Each talk is a stretch of its own.
	constexpr int32 InnsRoom = 105;
	constexpr uint16 TheNightBefore = 64;
	constexpr uint16 TheMorningAfter = 65;
	constexpr int32 BledaviksNotes = 0x82 / 2;
	// Registered for the tournament, Citan and Bart waiting in the room.
	constexpr uint16 RegisteredAndWaitedFor = 0xF000;
	constexpr int32 Citan = 2;
	std::vector<FGameState> Roots = StorySweepRoots(*Opened, {});
	REQUIRE(Roots.size() == 1);
	Roots[0].SetPlace(InnsRoom);
	Roots[0].SetSavedEventVariable(0, TheNightBefore);
	Roots[0].SetSavedEventVariable(BledaviksNotes, RegisteredAndWaitedFor);
	FStorySweepRun Run(*Opened, {}, &PortStory(), Options);
	std::vector<FToldStretch> Told;
	FStorySweepExplorer Explorer(Run, Options, [&](int32, uint16 Story, const std::string& Start, const FStorySweepRun&, const FStorySweepStretch& Stretch)
		{
			Told.push_back({Story, Start, Stretch});
		});
	Explorer.Explore(Roots[0], 1);
	const std::string TalkToCitan = "talk " + std::to_string(Citan);
	const auto First = std::find_if(Told.begin(), Told.end(), [&](const FToldStretch& Each) { return Each.Start == TalkToCitan; });
	REQUIRE(First != Told.end());
	// Asked whether to go out again, the first answer is yes: the evening goes on.
	CHECK(First->Stretch.Story == TheNightBefore);
	CHECK(std::any_of(Told.begin(), Told.end(), [&](const FToldStretch& Each) { return Each.Start == TalkToCitan + " again" && Each.Stretch.Story == TheMorningAfter; }));
}

TEST_CASE("A talk that opens the floor says so and the way it opened is the nearest zone", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FStorySweepOptions Options;
	Options.bForcesTriggers = false;
	Options.StretchFrames = 900;
	// Bledavik on the morning of the tournament, Bart alone by the inn's door. The well by the west houses is a lid
	// of the upper streets' floor: talked to, Bart lifts it, which closes its two triangles to that layer, and whoever
	// then walks onto them drops to the well's bottom, where the way into the waterway is. Loaded anew, the map has
	// the lid shut again.
	constexpr int32 Streets = 102;
	constexpr int32 WellsShaft = 108;
	constexpr uint16 TheMorningOfTheTournament = 67;
	constexpr std::array<uint8, 3> BartAlone = {3, 0xFF, 0xFF};
	constexpr uint16 ByTheInnsDoor = 3;
	constexpr int32 BledaviksNotes = 0x82 / 2;
	constexpr uint16 KnowsOfTheWells = 0x0040;
	constexpr int32 WestWell = 46;
	constexpr int32 WestWellsBottom = 7;
	std::vector<FGameState> Roots = StorySweepRoots(*Opened, {});
	REQUIRE(Roots.size() == 1);
	Roots[0].SetPlace(Streets);
	Roots[0].SetParty(BartAlone);
	Roots[0].SetFieldEntrance(ByTheInnsDoor);
	Roots[0].SetSavedEventVariable(0, TheMorningOfTheTournament);
	Roots[0].SetSavedEventVariable(BledaviksNotes, KnowsOfTheWells);
	FStorySweepRun Run(*Opened, {}, &PortStory(), Options);
	REQUIRE_FALSE(Run.Resume(Roots[0]));
	REQUIRE(Run.Play(Options.Scenes).End == EStorySweepEnd::Control);
	CHECK_FALSE(Run.FloorChanged());

	REQUIRE(Run.Ask({EStorySweepTrigger::Talk, WestWell}));
	const FStorySweepStretch Lifted = Run.Play(Options.Triggers);
	INFO(Lifted.Path << " | " << Lifted.Detail);
	REQUIRE(Lifted.End == EStorySweepEnd::Control);
	CHECK(Lifted.Map == Streets);
	CHECK(Run.FloorChanged());
	const std::vector<FStorySweepTrigger> Zones = Run.ZonesByNearness();
	REQUIRE_FALSE(Zones.empty());
	CHECK(Zones.front().Number == WestWellsBottom);

	REQUIRE(Run.Ask(Zones.front()));
	const FStorySweepStretch Dropped = Run.Play(Options.Triggers);
	INFO(Dropped.Path << " | " << Dropped.Detail);
	CHECK(Dropped.Path.rfind(std::to_string(Streets) + " > " + std::to_string(WellsShaft), 0) == 0);
}

TEST_CASE("A gate the map's scripts open by themselves is waited for and walked through", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FStorySweepOptions Options;
	Options.bForcesTriggers = false;
	Options.StretchFrames = 900;
	// Aveh's waterway after Bart's drop down Bledavik's well, Bart alone at the floodgate. The gate rises and falls
	// by itself, opening and shutting the strip of floor before the way on to the castle (attribute record 152) as
	// it goes: shut for some 1,040 frames, open for some 510.
	constexpr int32 Waterway = 130;
	constexpr int32 PastTheFloodgate = 147;
	constexpr uint16 InTheWaterway = 70;
	constexpr std::array<uint8, 3> BartAlone = {3, 0xFF, 0xFF};
	constexpr uint16 ByTheFloodgate = 4;
	constexpr int32 TheWayOn = 4;
	std::vector<FGameState> Roots = StorySweepRoots(*Opened, {});
	REQUIRE(Roots.size() == 1);
	Roots[0].SetPlace(Waterway);
	Roots[0].SetParty(BartAlone);
	Roots[0].SetFieldEntrance(ByTheFloodgate);
	Roots[0].SetSavedEventVariable(0, InTheWaterway);
	FStorySweepRun Run(*Opened, {}, &PortStory(), Options);
	REQUIRE_FALSE(Run.Resume(Roots[0]));
	REQUIRE(Run.Play(Options.Scenes).End == EStorySweepEnd::Control);

	REQUIRE(Run.Ask({EStorySweepTrigger::Area, TheWayOn}));
	const FStorySweepStretch Through = Run.Play(Options.Triggers);
	INFO(Through.Path << " | " << Through.Detail << " | " << Through.Barred);
	CHECK(Through.Opening == EStorySweepOpening::Answered);
	CHECK(Through.Path.rfind(std::to_string(Waterway) + " > " + std::to_string(PastTheFloodgate), 0) == 0);
}

TEST_CASE("Windows that open by themselves while the player keeps the party are read before a talk", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FStorySweepOptions Options;
	Options.bForcesTriggers = false;
	Options.StretchFrames = 900;
	// Aveh's castle below the arena, Bart alone up from the waterway. Two guards by the north doors talk among
	// themselves once he comes within 300 units, their windows taking the confirm button while he keeps his steps;
	// the doors to the stairs (actor 15, on to map 135) open to a talk once they are done. The castle's three doors
	// that the waterway's way in locks behind him (flags 864h to 866h) stay locked.
	constexpr int32 BelowTheArena = 133;
	constexpr int32 Stairs = 135;
	constexpr uint16 InTheCastle = 70;
	constexpr std::array<uint8, 3> BartAlone = {3, 0xFF, 0xFF};
	constexpr uint16 UpFromTheWaterway = 0;
	constexpr int32 CastleDoors = 0x86 / 2;
	constexpr uint16 LockedBehind = 0x0070;
	constexpr int32 DoorsToTheStairs = 15;
	std::vector<FGameState> Roots = StorySweepRoots(*Opened, {});
	REQUIRE(Roots.size() == 1);
	Roots[0].SetPlace(BelowTheArena);
	Roots[0].SetParty(BartAlone);
	Roots[0].SetFieldEntrance(UpFromTheWaterway);
	Roots[0].SetSavedEventVariable(0, InTheCastle);
	Roots[0].SetSavedEventVariable(CastleDoors, LockedBehind);
	FStorySweepRun Run(*Opened, {}, &PortStory(), Options);
	REQUIRE_FALSE(Run.Resume(Roots[0]));
	REQUIRE(Run.Play(Options.Scenes).End == EStorySweepEnd::Control);

	REQUIRE(Run.Ask({EStorySweepTrigger::Talk, DoorsToTheStairs}));
	const FStorySweepStretch Through = Run.Play(Options.Triggers);
	INFO(Through.Path << " | " << Through.Detail);
	CHECK(Through.Opening == EStorySweepOpening::Answered);
	CHECK(Through.Path.rfind(std::to_string(BelowTheArena) + " > " + std::to_string(Stairs), 0) == 0);
}

TEST_CASE("An actor is approached on the floor at its height, whatever its layer", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FStorySweepOptions Options;
	Options.bForcesTriggers = false;
	Options.StretchFrames = 900;
	// The gallery above the hall below Aveh's arena (map 133's layer 1), Bart come up the stairs to its north end.
	// Its doors never walk and keep layer 0, 528 units up: the gallery's north doors (actor 18) answer a talk there
	// and lead back to the stairs; from the hall below, the doors under them (actor 15) are the ones that answer.
	constexpr int32 BelowTheArena = 133;
	constexpr int32 Stairs = 135;
	constexpr uint16 InTheCastle = 70;
	constexpr std::array<uint8, 3> BartAlone = {3, 0xFF, 0xFF};
	constexpr uint16 GallerysNorthEnd = 8;
	constexpr int32 CastleDoors = 0x86 / 2;
	constexpr uint16 LockedBehind = 0x0070;
	constexpr int32 GallerysNorthDoors = 18;
	std::vector<FGameState> Roots = StorySweepRoots(*Opened, {});
	REQUIRE(Roots.size() == 1);
	Roots[0].SetPlace(BelowTheArena);
	Roots[0].SetParty(BartAlone);
	Roots[0].SetFieldEntrance(GallerysNorthEnd);
	Roots[0].SetSavedEventVariable(0, InTheCastle);
	Roots[0].SetSavedEventVariable(CastleDoors, LockedBehind);
	FStorySweepRun Run(*Opened, {}, &PortStory(), Options);
	REQUIRE_FALSE(Run.Resume(Roots[0]));
	REQUIRE(Run.Play(Options.Scenes).End == EStorySweepEnd::Control);

	REQUIRE(Run.Ask({EStorySweepTrigger::Talk, GallerysNorthDoors}));
	const FStorySweepStretch Through = Run.Play(Options.Triggers);
	INFO(Through.Path << " | " << Through.Detail << " | " << Through.Barred);
	CHECK(Through.Opening == EStorySweepOpening::Answered);
	CHECK(Through.Path.rfind(std::to_string(BelowTheArena) + " > " + std::to_string(Stairs), 0) == 0);
}

TEST_CASE("A place a scene walked the leader into past what it then shut is gone on from where a load stands him", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	FStorySweepOptions Options;
	Options.bForcesTriggers = false;
	Options.StretchFrames = 12000;
	Options.BattleFrames = 300;
	// Nortune's D-block before the "Baptismal Ceremony": map 218's scene gives Fei Rico for a second and moves on to
	// the hall of map 199, whose own walks him south through an unseen box to his mark and makes the box solid. Its
	// entrance 0 lies north of the box, its entrance 2, where its scripts have a loaded game resume, south of it: the
	// hall is gone on from by entrance 2. There the box asks "Hey, are you ready?" when touched, and only "Let's
	// rock", its second answer, begins the ceremony's fights.
	constexpr int32 DBlock = 218;
	constexpr int32 Hall = 199;
	constexpr uint16 BeforeTheCeremony = 104;
	constexpr std::array<uint8, 3> FeiAlone = {0, 0xFF, 0xFF};
	constexpr int32 Box = 2;
	constexpr int32 MemoryCube = 49;
	std::vector<FGameState> Roots = StorySweepRoots(*Opened, {});
	REQUIRE(Roots.size() == 1);
	Roots[0].SetPlace(DBlock);
	Roots[0].SetParty(FeiAlone);
	Roots[0].SetSavedEventVariable(0, BeforeTheCeremony);
	FStorySweepRun Run(*Opened, {}, &PortStory(), Options);
	std::vector<FToldStretch> Told;
	FStorySweepExplorer Explorer(Run, Options, [&](int32 Map, uint16 Story, const std::string& Start, const FStorySweepRun&, const FStorySweepStretch& Stretch)
		{
			if (Map == Hall)
			{
				Told.push_back({Story, Start, Stretch});
			}
		});
	Explorer.Explore(Roots[0], 2);
	const auto Line = [&](const std::string& Start) -> const FToldStretch*
	{
		const auto Found = std::find_if(Told.begin(), Told.end(), [&](const FToldStretch& Each) { return Each.Start == Start; });
		return Found != Told.end() ? &*Found : nullptr;
	};
	REQUIRE(Line("scene") != nullptr);
	const FToldStretch* Loaded = Line("scene at entrance 2");
	REQUIRE(Loaded != nullptr);
	CHECK(Loaded->Stretch.End == EStorySweepEnd::Control);
	const FToldStretch* Saved = Line("talk " + std::to_string(MemoryCube));
	REQUIRE(Saved != nullptr);
	CHECK(Saved->Stretch.Opening == EStorySweepOpening::Answered);
	const FToldStretch* Ready = Line("touch " + std::to_string(Box) + " again");
	REQUIRE(Ready != nullptr);
	INFO(Ready->Stretch.Path << " | " << Ready->Stretch.Detail);
	CHECK(Ready->Stretch.Path.rfind(std::to_string(Hall) + " > battle", 0) == 0);
}
