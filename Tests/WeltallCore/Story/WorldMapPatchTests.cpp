#include "Foundation/LittleEndian.h"
#include "Game/GameSession.h"
#include "Game/WorldMapProgram.h"
#include "Story/StoryPacks.h"
#include "Story/WorldMapPatch.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "Text/MesText.h"
#include "WorldMap/WorldMapContext.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace Weltall;

namespace
{
	/** The Mass Driver's ruins at the north tip of Taura's landmass, where the developers' early place list has them. */
	constexpr std::string_view MassDriver =
		"; The ruins at the north tip of Taura's landmass.\n"
		"world map 8\n"
		"message MassDriver\n"
		"{\n"
		"Mass Driver\n"
		"}\n"
		"place on foot x 29880 z 16287 width 202 height 214 to map 732 entrance 0 name MassDriver\n"
		"place in gears x 29880 z 16287 width 202 height 214 to map 732 entrance 1 name MassDriver\n"
		"arrival 40 x 29981 z 16394\n"
		"dot 21\n";

	/** A scene of two names, an entrance, a place on foot and none in gears. */
	FWorldMapScene SmallScene()
	{
		FWorldMapScene Scene;
		const std::vector<FAddedMessage> Names = {*MesMessageOfText("Lahan Village"), *MesMessageOfText("Mountain Path")};
		Scene.MessageBytes = *BuildMessageTable(Names);
		Scene.Entrances.push_back({100, 4, 200});
		Scene.TriggerLists[0] = std::vector<FWorldMapTrigger>{{10, 20, 30, 40, 1, 0, 0, 0}};
		Scene.TriggerLists[1] = std::vector<FWorldMapTrigger>{};
		return Scene;
	}

	std::string ErrorOf(std::string_view Text)
	{
		const TResult<FWorldMapPatch> Patch = FWorldMapPatch::Parse(Text);
		return Patch ? std::string() : Patch.Error().Message;
	}
}

TEST_CASE("A world map's places from the story follow its own, their names after its names", "[Story]")
{
	const TResult<FWorldMapPatch> Patch = FWorldMapPatch::Parse(
		"world map 8\nmessage Ruins\n{\nRuins\n}\nplace on foot x 1 z 2 width 3 height 4 to map 731 entrance 5 name Ruins\n"
		"place on foot x 11 z 12 width 13 height 14 on touch to map 731 entrance 6 name #1\n"
		"place in gears x 1 z 2 width 3 height 4 to map 731 entrance 7 name #1\narrival 7 x 8 z 9\ndot 21\ndot 14\n");
	INFO((Patch ? std::string() : Patch.Error().Message));
	REQUIRE(Patch);
	CHECK(Patch->Era() == 8);
	FWorldMapScene Scene = SmallScene();
	REQUIRE_FALSE(Patch->Apply(Scene));
	REQUIRE(Scene.TriggerLists[0]->size() == 3);
	CHECK((*Scene.TriggerLists[0])[1] == FWorldMapTrigger{1, 2, 3, 4, 731, 5, 2, 0});
	CHECK((*Scene.TriggerLists[0])[2] == FWorldMapTrigger{11, 12, 13, 14, 731, 6, 1, 1});
	REQUIRE(Scene.TriggerLists[1]->size() == 1);
	CHECK(Scene.TriggerLists[1]->front() == FWorldMapTrigger{1, 2, 3, 4, 731, 7, 1, 0});
	REQUIRE(Scene.Entrances.size() == 2);
	CHECK(Scene.Entrances.back().Id == 7);
	CHECK(Scene.Entrances.back().X == 8);
	CHECK(Scene.Entrances.back().Z == 9);
	const auto Messages = Scene.Messages();
	REQUIRE(Messages);
	REQUIRE(Messages->Count() == 3);
	CHECK(MesTextOfCodes(Messages->String(2)) == "Ruins");
	CHECK(Scene.StoryDots == (1u << 21 | 1u << 14));
}

TEST_CASE("A world map's places that cannot be read or added are refused", "[Story]")
{
	CHECK(ErrorOf("place on foot x 1 z 2 width 3 height 4 to map 731 entrance 5 name #1\n") == "line 1: the text begins 'world map E'");
	CHECK(ErrorOf("world map 9\n") == "line 1: 'world map E' takes the era, 0 to 8, once");
	CHECK(ErrorOf("world map 8\nplace on foot x 1 z 2 width 3 to map 731 entrance 5 name #1\n").starts_with("line 2: a place reads"));
	CHECK(ErrorOf("world map 8\nplace in vehicle x 1 z 2 width 3 height 4 on touch to map 731 entrance 5 name #1\n").starts_with("line 2: a place reads"));
	CHECK(ErrorOf("world map 8\nplace on foot x 1 z 2 width 3 height 4 to map 731 entrance 5 name Ruins\n")
		== "line 2: a place's name is a message before it, or one of the scene's own by number (#20)");
	CHECK(ErrorOf("world map 8\nmessage Long\n{\nThe Mass Driver's Ruins\n}\n") == "line 5: a place's name is one line of 16 letters at most");
	CHECK(ErrorOf("world map 8\nmessage Open\n{\nRuins\n") == "the name 'Open' has no text");
	CHECK(ErrorOf("world map 8\narrival 4095 x 1 z 2\n") == "line 2: an arrival reads 'arrival N x X z Z'");
	// The minimap's dots after 23 are the ship's, Shevat's and the Thames', which move.
	CHECK(ErrorOf("world map 8\ndot 24\n") == "line 2: a dot reads 'dot N', one of the minimap's places, 0 to 23");
	CHECK(ErrorOf("world map 8\ndot\n") == "line 2: a dot reads 'dot N', one of the minimap's places, 0 to 23");
	CHECK(ErrorOf("world map 8\ndot 21 22\n") == "line 2: a dot reads 'dot N', one of the minimap's places, 0 to 23");

	FWorldMapScene Scene = SmallScene();
	const auto Taken = FWorldMapPatch::Parse("world map 8\narrival 4 x 1 z 2\n");
	REQUIRE(Taken);
	const std::optional<FError> TakenError = Taken->Apply(Scene);
	REQUIRE(TakenError);
	CHECK(TakenError->Message == "arrival 4 is one of the world map's own");
	const auto Unnamed = FWorldMapPatch::Parse("world map 8\nplace on foot x 1 z 2 width 3 height 4 to map 731 entrance 5 name #2\n");
	REQUIRE(Unnamed);
	const std::optional<FError> UnnamedError = Unnamed->Apply(Scene);
	REQUIRE(UnnamedError);
	CHECK(UnnamedError->Message == "the world map has no name 2");
	const auto NoList = FWorldMapPatch::Parse("world map 8\nplace in vehicle x 1 z 2 width 3 height 4 to map 731 entrance 5 name #1\ndot 3\n");
	REQUIRE(NoList);
	const std::optional<FError> NoListError = NoList->Apply(Scene);
	REQUIRE(NoListError);
	CHECK(NoListError->Message == "the world map has no list of places in vehicle");
	// Nothing of a refused document is added.
	CHECK(Scene.TriggerLists[0]->size() == 1);
	CHECK(Scene.Entrances.size() == 1);
	CHECK(Scene.StoryDots == 0);
}

TEST_CASE("A world map's text keeps the party's ship away at the story's values it names, the expansion's only in a game of it", "[Story]")
{
	const auto Patch = FWorldMapPatch::Parse("world map 8\nvehicle away at story 253\nvehicle away at story 260-262\n");
	INFO((Patch ? std::string() : Patch.Error().Message));
	REQUIRE(Patch);
	CHECK_FALSE(Patch->KeepsVehicleAway(252));
	CHECK(Patch->KeepsVehicleAway(253));
	CHECK_FALSE(Patch->KeepsVehicleAway(254));
	CHECK(Patch->KeepsVehicleAway(260));
	CHECK(Patch->KeepsVehicleAway(262));
	CHECK_FALSE(Patch->KeepsVehicleAway(263));
	CHECK(ErrorOf("world map 8\nvehicle away at story 262-260\n") == "line 2: the ship away reads 'vehicle away at story S', or S-T");
	CHECK(ErrorOf("world map 8\nvehicle away 253\n") == "line 2: the ship away reads 'vehicle away at story S', or S-T");

	FStoryPacks Packs;
	REQUIRE_FALSE(Packs.Add(FStoryPacks::ExpansionPack, "away.txt", "world map 8\nvehicle away at story 253\n"));
	CHECK(Packs.KeepsVehicleAway(8, 253, true));
	CHECK_FALSE(Packs.KeepsVehicleAway(8, 253, false));
	CHECK_FALSE(Packs.KeepsVehicleAway(7, 253, true));
	CHECK_FALSE(Packs.KeepsVehicleAway(8, 254, true));
}

TEST_CASE("The expanded second disc's world map places are a game's only when it plays it, in their era", "[Story]")
{
	FStoryPacks Packs;
	REQUIRE_FALSE(Packs.Add(FStoryPacks::ExpansionPack, "mass-driver.txt", MassDriver));
	CHECK(Packs.HasExpansion());
	CHECK(Packs.DocumentCount() == 1);
	FWorldMapScene Era8 = SmallScene();
	REQUIRE_FALSE(Packs.ApplyToWorldMap(8, Era8, false));
	CHECK(Era8.TriggerLists[0]->size() == 1);
	REQUIRE_FALSE(Packs.ApplyToWorldMap(7, Era8, true));
	CHECK(Era8.TriggerLists[0]->size() == 1);
	REQUIRE_FALSE(Packs.ApplyToWorldMap(8, Era8, true));
	CHECK(Era8.TriggerLists[0]->size() == 2);
	// A scene that cannot take a document names it.
	FWorldMapScene Again = Era8;
	const std::optional<FError> Twice = Packs.ApplyToWorldMap(8, Again, true);
	REQUIRE(Twice);
	CHECK(Twice->Message == "disc2/mass-driver.txt: arrival 40 is one of the world map's own");
	const auto Broken = Packs.Add(FStoryPacks::ExpansionPack, "broken.txt", "world map 8\nplace\n");
	REQUIRE(Broken);
	CHECK(Broken->Message.starts_with("disc2/broken.txt: line 2: a place reads"));
}

TEST_CASE("On the second disc's world map a game of the expanded second disc arrives at a place of the story's and enters it", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	FStoryPacks Packs;
	REQUIRE_FALSE(Packs.Add(FStoryPacks::ExpansionPack, "mass-driver.txt", MassDriver));
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FGameSession& Session = *Game->Session;
	Session.SetStory(&Packs);
	FGameState& State = Session.Game();
	// The story after the rematch, era 8, arriving by the story's own entrance.
	State.SetStoryMode(EStoryMode::Expanded);
	State.SetSavedEventVariable(0, 253);
	State.SetWorldMapPlace(0x401, 40, 0xC00, 1);
	auto Started = FWorldMapProgram::Start(Session);
	INFO((Started ? std::string() : Started.Error().Message));
	REQUIRE(Started);
	FWorldMapProgram& Program = **Started;
	const auto Frame = [&Program](FPadState Held)
	{
		const std::vector<FPadState> Vblanks(static_cast<std::size_t>(Program.VblanksPerFrame()), Held);
		REQUIRE_FALSE(Program.Frame(Vblanks));
	};
	for (int32 Tick = 0; Tick < 200; ++Tick)
	{
		Frame(FPadState{});
	}
	const FWorldMapContext& Context = Program.Runtime().Context();
	CHECK(Context.Era == 8);
	REQUIRE(Context.Messages);
	const FMessageTable& Names = *Context.Messages;
	REQUIRE(Names.Count() > 0);
	CHECK(MesTextOfCodes(Names.String(Names.Count() - 1)) == "Mass Driver");
	// The minimap shows the place's dot, which the game has not shown: the developers' own on the ruins, 90 and 49
	// pixels from the minimap's corner.
	CHECK((LittleEndian::ReadU32(State.Block(), 0x1B2C) >> 21 & 1) == 0);
	const FWorldMapFrame* Shown = Program.LatestFrame();
	REQUIRE(Shown != nullptr);
	const auto Dot = std::find_if(Shown->Minimap.Primitives.begin(), Shown->Minimap.Primitives.end(),
		[](const FDrawPrimitive& Primitive) { return Primitive.BlendKey == WorldMapBlendKeys::MinimapDots + 21; });
	REQUIRE(Dot != Shown->Minimap.Primitives.end());
	CHECK(Dot->Corners[0].X == 208 + 90);
	CHECK(Dot->Corners[0].Y == 120 + 49);
	// Standing on the place, the confirm button enters it: the field's map and entrance.
	for (int32 Tick = 0; Tick < 200 && Program.State() == EGameProgramState::Running; ++Tick)
	{
		Frame(Tick % 4 == 0 ? FPadState{FPadState::Bit(EPadButton::Cross)} : FPadState{});
	}
	CHECK(Program.State() == EGameProgramState::Ended);
	CHECK(Session.Kernel().NextProgram == EGameProgram::Field);
	CHECK(State.Place() == 732);
	CHECK(State.Scene() == 0);
}
