#include "Game/GameState.h"
#include "Story/StoryPacks.h"
#include "Story/StoryWayBack.h"

#include <catch_amalgamated.hpp>

#include <string>

using namespace Weltall;

namespace
{
	constexpr std::string_view Table =
		"; Where a game of the expanded second disc goes on in the original's.\n"
		"way back\n"
		"story 253 to map 617 entrance 2 story 252\n"
		"\n"
		"story 254-257 to map 618 entrance 0 story 252   ; the front and the battleship\n";

	std::string ErrorOf(std::string_view Text)
	{
		const TResult<FStoryWayBack> WayBack = FStoryWayBack::Parse(Text);
		return WayBack ? std::string() : WayBack.Error().Message;
	}
}

TEST_CASE("A way back names the original's point for each value of the expanded play's story", "[Story]")
{
	const TResult<FStoryWayBack> WayBack = FStoryWayBack::Parse(Table);
	REQUIRE(WayBack);
	const FWayBackPoint* Chair = WayBack->For(253);
	REQUIRE(Chair != nullptr);
	CHECK(Chair->Map == 617);
	CHECK(Chair->Entrance == 2);
	CHECK(Chair->Story == 252);
	for (const uint16 Story : {uint16{254}, uint16{256}, uint16{257}})
	{
		const FWayBackPoint* Front = WayBack->For(Story);
		REQUIRE(Front != nullptr);
		CHECK(Front->Map == 618);
		CHECK(Front->Entrance == 0);
	}
	// Where the expanded play is the original's own, there is no other point.
	CHECK(WayBack->For(252) == nullptr);
	CHECK(WayBack->For(258) == nullptr);

	// Only for a game of the expanded second disc.
	FGameState State;
	State.SetSavedEventVariable(0, 253);
	CHECK(WayBack->For(State) == nullptr);
	State.SetStoryMode(EStoryMode::Original);
	CHECK(WayBack->For(State) == nullptr);
	State.SetStoryMode(EStoryMode::Expanded);
	CHECK(WayBack->For(State) == Chair);

	// The point puts a game at the map's entrance, the story at its value.
	State.SetPlace(731);
	State.SetFieldEntrance(5);
	State.SetSavedEventVariable(1, 5);
	Chair->Apply(State);
	CHECK(State.Place() == 617);
	CHECK(State.Scene() == 2);
	CHECK(State.SavedEventVariable(1) == 2);
	CHECK(State.SavedEventVariable(0) == 252);
}

TEST_CASE("A way back that cannot be read is refused with its line", "[Story]")
{
	CHECK(ErrorOf("story 253 to map 617 entrance 2 story 252\n") == "line 1: the text begins 'way back'");
	CHECK(ErrorOf("; nothing\n") == "the text begins 'way back'");
	CHECK(ErrorOf("way back\nstory 253 to map 617 entrance 2\n").starts_with("line 2: a row reads"));
	CHECK(ErrorOf("way back\nstory 257-254 to map 617 entrance 2 story 252\n").starts_with("line 2: a row reads"));
	CHECK(ErrorOf("way back\nstory 253 to map 731 entrance 0 story 252\n") == "line 2: the point is a map of the disc's own, below 730");
	CHECK(ErrorOf("way back\nstory 253-255 to map 617 entrance 2 story 252\nstory 255 to map 618 entrance 0 story 252\n")
		== "line 3: a value of the story another row names already");
}

TEST_CASE("The expanded second disc's pack may say where its games go on in the original's, once", "[Story]")
{
	FStoryPacks Packs;
	CHECK(Packs.WayBack() == nullptr);
	REQUIRE_FALSE(Packs.Add(FStoryPacks::ExpansionPack, "way-back.txt", Table));
	REQUIRE(Packs.WayBack() != nullptr);
	CHECK(Packs.WayBack()->For(253) != nullptr);
	CHECK(Packs.DocumentCount() == 1);

	const auto Again = Packs.Add(FStoryPacks::ExpansionPack, "again.txt", Table);
	REQUIRE(Again);
	CHECK(Again->Message == "disc2/again.txt: the way back is the expanded second disc's, one document of its pack");
	FStoryPacks Elsewhere;
	const auto Other = Elsewhere.Add("additions", "way-back.txt", Table);
	REQUIRE(Other);
	CHECK(Other->Message == "additions/way-back.txt: the way back is the expanded second disc's, one document of its pack");
	const auto Broken = Elsewhere.Add(FStoryPacks::ExpansionPack, "broken.txt", "way back\nstory\n");
	REQUIRE(Broken);
	CHECK(Broken->Message.starts_with("disc2/broken.txt: line 2: a row reads"));
}
