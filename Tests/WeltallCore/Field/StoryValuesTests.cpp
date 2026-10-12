#include "Field/EventAssembly.h"
#include "Support/StoryValues.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;
using namespace Weltall::Testing;

TEST_CASE("A map's scripts are read for what they compare the story's progress with and the maps they leave for", "[Field]")
{
	// The progress compared by its own instructions and by If from either side; another variable compared, a map change
	// by a variable's value and an instruction no thread reaches, none of which count.
	const auto Section = AssembleEventScripts(
		"actors 2\n"
		"actor 0 script 0:\n"
		"    IfAccLess #6 Later\n"
		"    ChangeMap #13 #0\n"
		"Later:\n"
		"    IfAccEqual var[400] Later\n"
		"    If var[000] >= #21 else Door\n"
		"    If #130 < var[001] else Door\n"
		"    If var[2C2] == #77 else Door\n"
		"    ChangeMapTransition #3 #1 #0 #30\n"
		"Door:\n"
		"    EnterDoor 1 #6 #0\n"
		"    ChangeMap var[402] #0\n"
		"    End\n"
		"    IfAccGreater #99 Door\n"
		"actor 1 script 2:\n"
		"    IfAccGreater #44 Out\n"
		"    DoorExit 0 #700 #2\n"
		"Out:\n"
		"    End\n");
	REQUIRE(Section);
	const auto Scripts = FEventScripts::Parse(*Section);
	REQUIRE(Scripts);
	const FStoryReferences Found = FindStoryReferences(*Scripts);
	CHECK(Found.Compared == std::set<uint16>{6, 21, 44, 130});
	CHECK(Found.Neighbours == std::set<int32>{3, 6, 13, 700});
}

TEST_CASE("A map's scripts are read for the walk attribute records they change", "[Field]")
{
	// Set, or'ed and and'ed bytes of a record count; a byte only read does not, nor a write no thread reaches.
	const auto Section = AssembleEventScripts(
		"actors 1\n"
		"actor 0 script 1:\n"
		"    SetWalkAttrByte 152 0 #248\n"
		"    OrWalkAttrByte 36 1 #32\n"
		"    AndWalkAttrByte 9 0 #0\n"
		"    GetWalkAttrByte 40 0 var[402]\n"
		"    End\n"
		"    SetWalkAttrByte 77 0 #0\n");
	REQUIRE(Section);
	const auto Scripts = FEventScripts::Parse(*Section);
	REQUIRE(Scripts);
	CHECK(FindStoryReferences(*Scripts).WalkRecords == std::set<int32>{9, 36, 152});
}

TEST_CASE("The story's values told apart are each one compared with and one of each run between them", "[Field]")
{
	CHECK(StoryValuesToldApart({6}) == std::vector<uint16>{5, 6, 7});
	CHECK(StoryValuesToldApart({0, 1, 5}) == std::vector<uint16>{0, 1, 2, 5, 6});
	CHECK(StoryValuesToldApart({24, 26, 30}) == std::vector<uint16>{23, 24, 25, 26, 27, 30, 31});
}
