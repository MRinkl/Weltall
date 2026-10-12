#include "Field/FieldMap.h"
#include "Field/FieldMapPatch.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"
#include "Text/MesText.h"
#include "Text/MessageTable.h"

#include <catch_amalgamated.hpp>

#include <string>

using namespace Weltall;

// Every map of both discs written out as the text of a map of its own over it, and that text built: the map's own
// script section and messages come back, so a map of one's own can start from any map's text.

namespace
{
	/** How many of maps First to End - 1 of a disc are maps on it (the second disc holds placeholders for those it never loads), each sent through its text. */
	int32 RoundTrip(const FIso9660& Disc, int32 First, int32 End)
	{
		constexpr int32 OwnNumber = 1000;
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		int32 Maps = 0;
		for (int32 Number = First; Number < End; ++Number)
		{
			INFO("map " << Number);
			const auto Archive = LoadFieldArchive(Disc.Image(), *Index, Number);
			if (!Archive)
			{
				continue;
			}
			++Maps;
			const std::vector<uint8>& Section = Archive->Section(EFieldSection::EventScripts);
			const std::vector<uint8>& Messages = Archive->Section(EFieldSection::Messages);
			const auto Text = WriteFieldMapText(OwnNumber, Number, Section, Messages);
			REQUIRE(Text);
			const auto Document = FFieldMapPatch::Parse(*Text);
			if (!Document)
			{
				FAIL(Document.Error().Message);
			}
			CHECK(Document->Map() == OwnNumber);
			CHECK(Document->Base() == Number);
			const auto Built = Document->Build();
			if (!Built)
			{
				FAIL(Built.Error().Message);
			}
			// Compared apart from the check's own expansion, which would print both sections whole.
			const bool bSameScripts = Built->EventScripts == Section;
			CHECK(bSameScripts);
			const auto Own = FMessageTable::Parse(Messages);
			const auto Again = FMessageTable::Parse(Built->Messages);
			REQUIRE(Own);
			REQUIRE(Again);
			REQUIRE(Again->Count() == Own->Count());
			for (int32 Message = 0; Message < Own->Count(); ++Message)
			{
				const bool bSameMessage = MesTextOfCodes(Again->String(Message)) == MesTextOfCodes(Own->String(Message))
					&& Again->WidthUnits(Message) == Own->WidthUnits(Message) && Again->Lines(Message) == Own->Lines(Message);
				if (!bSameMessage)
				{
					FAIL("message " << Message << ": " << MesTextOfCodes(Own->String(Message)));
				}
			}
		}
		return Maps;
	}
}

TEST_CASE("Every map of the first disc comes back from its text as a map of its own: maps 0 to 182", "[Field][Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CHECK(RoundTrip(*Disc, 0, 183) == 183);
}

TEST_CASE("Every map of the first disc comes back from its text as a map of its own: maps 183 to 365", "[Field][Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CHECK(RoundTrip(*Disc, 183, 366) == 183);
}

TEST_CASE("Every map of the first disc comes back from its text as a map of its own: maps 366 to 547", "[Field][Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CHECK(RoundTrip(*Disc, 366, 548) == 182);
}

TEST_CASE("Every map of the first disc comes back from its text as a map of its own: maps 548 to 729", "[Field][Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CHECK(RoundTrip(*Disc, 548, FieldMapCount) == 182);
}

TEST_CASE("Every map the second disc holds comes back from its text as a map of its own", "[Field][Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	CHECK(RoundTrip(*Disc, 0, FieldMapCount) == 205);
}
