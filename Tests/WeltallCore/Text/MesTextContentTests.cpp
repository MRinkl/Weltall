#include "Field/FieldMap.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"
#include "Text/MesText.h"

#include <catch_amalgamated.hpp>

#include <algorithm>

using namespace Weltall;

// Every message of every map on both discs, written out as text and read back: the marks cover all of the script.

namespace
{
	struct FMessageCensus
	{
		int32 Maps = 0;
		int32 Messages = 0;
		/** The messages whose window is their text's own widest line and tallest page. */
		int32 SizedByTheirText = 0;
	};

	FMessageCensus RoundTrip(const FIso9660& Disc)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		FMessageCensus Census;
		for (int32 Number = 0; Number < FieldMapCount; ++Number)
		{
			const auto Archive = LoadFieldArchive(Disc.Image(), *Index, Number);
			if (!Archive)
			{
				continue;
			}
			const auto Table = FMessageTable::Parse(Archive->Section(EFieldSection::Messages));
			REQUIRE(Table);
			++Census.Maps;
			for (int32 Message = 0; Message < Table->Count(); ++Message)
			{
				const std::span<const uint8> Stored = Table->String(Message);
				const std::string Text = MesTextOfCodes(Stored);
				const auto Codes = MesCodesOfText(Text);
				if (!Codes || Codes->size() > Stored.size() || !std::equal(Codes->begin(), Codes->end(), Stored.begin()))
				{
					FAIL("map " << Number << " message " << Message << ": " << Text);
				}
				const FMesExtent Extent = MesExtentOfCodes(Stored);
				Census.SizedByTheirText += Extent.WidthUnits == Table->WidthUnits(Message) && Extent.Lines == Table->Lines(Message);
				++Census.Messages;
			}
		}
		return Census;
	}
}

TEST_CASE("Every map's messages on the first disc read back from their text", "[Text][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMessageCensus Census = RoundTrip(*Disc);
	CHECK(Census.Maps == FieldMapCount);
	CHECK(Census.Messages == 18182);
	CHECK(Census.SizedByTheirText == 17115);
}

TEST_CASE("Every map's messages on the second disc read back from their text", "[Text][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	const FMessageCensus Census = RoundTrip(*Disc);
	CHECK(Census.Maps == 205);
	CHECK(Census.Messages == 6154);
	CHECK(Census.SizedByTheirText == 5778);
}
