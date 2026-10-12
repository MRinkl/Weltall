#include "Story/BattleEventTextPatch.h"
#include "Text/MesText.h"
#include "Text/MessageTable.h"

#include <catch_amalgamated.hpp>

#include <string>
#include <string_view>
#include <vector>

using namespace Weltall;

namespace
{
	using FBytes = std::vector<uint8>;

	/** An event's two messages: a narration of two pages, and a line. */
	FBytes Table()
	{
		const std::vector<FAddedMessage> Own = {*MesMessageOfText("Met him again.{page}\nHe stood\nin my way."), *MesMessageOfText("No choice.")};
		return *BuildMessageTable(Own);
	}

	FBattleEventTextPatch Patch(std::string_view Text)
	{
		const auto Read = FBattleEventTextPatch::Parse(Text);
		REQUIRE(Read);
		return *Read;
	}

	std::string Refused(std::string_view Text)
	{
		const auto Read = FBattleEventTextPatch::Parse(Text);
		return Read ? std::string("read") : Read.Error().Message;
	}

	/** Message Index of a table made by a patch, as text. */
	std::string Told(const std::optional<FBytes>& Changed, int32 Index)
	{
		REQUIRE(Changed);
		const auto Read = FMessageTable::Parse(*Changed);
		REQUIRE(Read);
		return MesTextOfCodes(Read->String(Index));
	}
}

TEST_CASE("A battle event's text begins a message at a later page of its own", "[Story]")
{
	const FBytes Disc = Table();
	const auto Whole = FMessageTable::Parse(Disc);
	REQUIRE(Whole);
	const FBattleEventTextPatch Later = Patch("; The first page is told elsewhere.\n\nbattle event 19\nmessage 0 from page 2\n");
	CHECK(Later.Event() == 19);
	const auto Changed = Later.Apply(Disc);
	REQUIRE(Changed);
	const auto Read = FMessageTable::Parse(*Changed);
	REQUIRE(Read);
	REQUIRE(Read->Count() == 2);
	// The words are the table's own from the page on, in the window the message was written for; the rest is as it was.
	CHECK(MesTextOfCodes(Read->String(0)) == "He stood\nin my way.");
	CHECK(Read->WidthUnits(0) == Whole->WidthUnits(0));
	CHECK(Read->Lines(0) == Whole->Lines(0));
	CHECK(MesTextOfCodes(Read->String(1)) == "No choice.");
	CHECK(Read->WidthUnits(1) == Whole->WidthUnits(1));

	// From its first page a message is whole.
	CHECK(Told(Patch("battle event 19\nmessage 0 from page 1\n").Apply(Disc), 0) == "Met him again.{page}\nHe stood\nin my way.");

	// A table without the page or the message is another than the one written for: nothing is made of it.
	CHECK_FALSE(Patch("battle event 19\nmessage 0 from page 3\n").Apply(Disc));
	CHECK_FALSE(Patch("battle event 19\nmessage 2 from page 1\n").Apply(Disc));
	CHECK_FALSE(Later.Apply(FBytes{}));
}

TEST_CASE("A battle event's text tells pages of its own before a message's", "[Story]")
{
	const FBytes Disc = Table();
	const std::string_view Text =
		"battle event 19\n"
		"message 0 from page 2 after\n"
		"{\n"
		"He caught up.{wait 1E}\n"
		"   We fell.\n"
		"}\n"
		"message 1 from page 1 after\n"
		"{\n"
		"One.{page}Two.\n"
		"}\n";
	const auto Changed = Patch(Text).Apply(Disc);
	// Its own pages, then the message's from the page named, each on a new page as the game's follow one another.
	CHECK(Told(Changed, 0) == "He caught up.{wait 1E}\n   We fell.{page}\nHe stood\nin my way.");
	CHECK(Told(Changed, 1) == "One.{page}Two.{page}\nNo choice.");

	// What is told first is typed in the message's own window: wider or taller than it, the table is not the one written for.
	CHECK_FALSE(Patch("battle event 19\nmessage 1 from page 1 after\n{\nA line too wide for it.\n}\n").Apply(Disc));
	CHECK_FALSE(Patch("battle event 19\nmessage 1 from page 1 after\n{\nTwo\nlines.\n}\n").Apply(Disc));
	CHECK(Patch("battle event 19\nmessage 1 from page 1 after\n{\nShort.\n}\n").Apply(Disc));
}

TEST_CASE("A battle event's text that cannot be read is refused with its line", "[Story]")
{
	CHECK(Refused("message 0 from page 2\n") == "the text names no battle event");
	CHECK(Refused("battle event\n") == "line 1: 'battle event N' takes the event's number, once");
	CHECK(Refused("battle event 256\n") == "line 1: 'battle event N' takes the event's number, once");
	CHECK(Refused("battle event 1\nbattle event 2\n") == "line 2: 'battle event N' takes the event's number, once");
	const std::string_view Beginning =
		"line 2: a message's beginning reads 'message K from page P', the pages counted from 1, or with 'after' and the text told first";
	CHECK(Refused("battle event 1\nmessage 0\n") == Beginning);
	CHECK(Refused("battle event 1\nmessage 0 from page 0\n") == Beginning);
	CHECK(Refused("battle event 1\nmessage 0 at page 2\n") == Beginning);
	CHECK(Refused("battle event 1\nmessage 0 from page 2 before\n") == Beginning);
	CHECK(Refused("battle event 1\nmessage 0 from page 2\nmessage 0 from page 3\n") == "line 3: a message given two beginnings");
	CHECK(Refused("battle event 1\nactor 0 script 0:\n") == "line 2: a battle event's text takes 'battle event N' and its messages' beginnings");
	CHECK(Refused("battle event 1\nmessage 0 from page 2 after\nHe caught up.\n}\n") == "line 3: the text told first begins after a line of '{'");
	CHECK(Refused("battle event 1\nmessage 0 from page 2 after\n{\nHe caught up~\n}\n") == "line 4: a character or a mark the messages lack");
	CHECK(Refused("battle event 1\nmessage 0 from page 2 after\n{\nHe caught up.\n") == "the text told first does not end with a line of '}'");
	CHECK(Refused("battle event 1\n; nothing changed\n") == "read");
}
