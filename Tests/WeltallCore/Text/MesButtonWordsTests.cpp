#include "Text/MesButtonWords.h"
#include "Text/MesGlyphs.h"

#include <catch_amalgamated.hpp>

#include <string_view>
#include <vector>

using namespace Weltall;

namespace
{
	/** A typed line of plain text, each glyph two units on from the one before. */
	std::vector<FMesPlacedGlyph> Typed(std::string_view Text)
	{
		std::vector<FMesPlacedGlyph> Glyphs;
		for (const char Character : Text)
		{
			Glyphs.push_back({*MesCodeOfCharacter(Character), static_cast<int16>(2 * Glyphs.size())});
		}
		return Glyphs;
	}

	std::vector<FMesButtonWord> Find(std::string_view Text)
	{
		std::vector<FMesButtonWord> Words;
		FindMesButtonWords(Typed(Text), Words);
		return Words;
	}
}

TEST_CASE("A line's buttons named in words are found with where their names lie", "[Text]")
{
	std::vector<FMesButtonWord> Words = Find("Press the 'R1 button' to");
	REQUIRE(Words.size() == 1);
	CHECK(Words[0].Button == EPadButton::R1);
	CHECK(Words[0].FirstGlyph == 11);
	CHECK(Words[0].GlyphCount == 2);
	CHECK(Words[0].EndUnits == 26);
	CHECK_FALSE(Words[0].bDirections);

	Words = Find("press the L2 and R2 buttons");
	REQUIRE(Words.size() == 2);
	CHECK(Words[0].Button == EPadButton::L2);
	CHECK(Words[1].Button == EPadButton::R2);

	Words = Find("Left, Right Directional Button: move");
	REQUIRE(Words.size() == 1);
	CHECK(Words[0].bDirections);
	CHECK(Words[0].Button == EPadButton::Up);
	CHECK(Words[0].GlyphCount == 11);

	CHECK(Find("The Start button will").size() == 1);
	CHECK(Find("By pushing the Select buttons,").size() == 1);
}

TEST_CASE("Only a whole button's name followed by the word button is a button named in words", "[Text]")
{
	CHECK(Find("Start the engine").empty());
	CHECK(Find("Select one").empty());
	CHECK(Find("XL1 button").empty());
	CHECK(Find("the L1 buttonhole").empty());
	CHECK(Find("L1 and the R1 button").size() == 1);
	CHECK(Find("Starting button").empty());
}

TEST_CASE("A button's name is found with its word on the next line, read on past the line's end", "[Text]")
{
	// "That's why we have the L1 / button to GUARD you!" (the arena's Informationist), and map 202's "the L2 and / R2 buttons".
	std::vector<FMesPlacedGlyph> Guard = Typed("we have the L1 ");
	Guard.push_back({MesLineEnd, static_cast<int16>(2 * Guard.size())});
	for (const FMesPlacedGlyph& Glyph : Typed("button to GUARD you!"))
	{
		Guard.push_back({Glyph.Code, static_cast<int16>(Glyph.XUnits + 2 * Guard.size())});
	}
	std::vector<FMesButtonWord> Words;
	FindMesButtonWords(Guard, Words);
	REQUIRE(Words.size() == 1);
	CHECK(Words[0].Button == EPadButton::L1);
	CHECK(Words[0].FirstGlyph == 12);
	CHECK(Words[0].EndUnits == 28);

	std::vector<FMesPlacedGlyph> Pair = Typed("pressing the L2 and");
	Pair.push_back({MesLineEnd, static_cast<int16>(2 * Pair.size())});
	for (const FMesPlacedGlyph& Glyph : Typed("R2 buttons simultaneously!!"))
	{
		Pair.push_back({Glyph.Code, static_cast<int16>(Glyph.XUnits + 2 * Pair.size())});
	}
	FindMesButtonWords(Pair, Words);
	REQUIRE(Words.size() == 2);
	CHECK(Words[0].Button == EPadButton::L2);
	CHECK(Words[1].Button == EPadButton::R2);

	// A line that ends on a name whose next line says something else names no button.
	std::vector<FMesPlacedGlyph> Other = Typed("the L1");
	Other.push_back({MesLineEnd, static_cast<int16>(2 * Other.size())});
	for (const FMesPlacedGlyph& Glyph : Typed("guards"))
	{
		Other.push_back({Glyph.Code, static_cast<int16>(Glyph.XUnits + 2 * Other.size())});
	}
	FindMesButtonWords(Other, Words);
	CHECK(Words.empty());
}
