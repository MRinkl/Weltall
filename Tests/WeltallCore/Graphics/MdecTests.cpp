#include "Graphics/Mdec.h"

#include <catch_amalgamated.hpp>

#include <cmath>

using namespace Weltall;

namespace
{
	constexpr uint16 EndOfBlock = 0xFE00;

	uint16 Dc(uint16 QuantScale, int32 Value)
	{
		return static_cast<uint16>(QuantScale << 10 | (Value & 0x3FF));
	}

	/** One macroblock: flat chroma blocks of ChromaDc, flat luma blocks of LumaDc. */
	void AddFlatMacroblock(std::vector<uint16>& Codes, int32 LumaDc, int32 ChromaDc = 0)
	{
		for (int32 Block = 0; Block < 6; ++Block)
		{
			Codes.push_back(Dc(1, Block < 2 ? ChromaDc : LumaDc));
			Codes.push_back(EndOfBlock);
		}
	}
}

TEST_CASE("An MDEC block's DC value is its flat level, an eighth of the weighted coefficient", "[Graphics]")
{
	// DC 64 weighed by the intra matrix's 2 is 128; flat at 128 / 8 = 16 levels, 256 in sixteenths.
	std::vector<uint16> Codes;
	AddFlatMacroblock(Codes, 64);
	const auto Picture = DecodeMdecPicture(Codes, 16, 16, FMdecTables::Standard());
	REQUIRE(Picture);
	CHECK(Picture->Luma.size() == 256);
	for (const int16 Sample : Picture->Luma)
	{
		CHECK(Sample == 256);
	}
	for (const int16 Sample : Picture->ChromaRed)
	{
		CHECK(Sample == 0);
	}
	const std::vector<uint8> Rgb = ToRgb24(*Picture);
	REQUIRE(Rgb.size() == 16 * 16 * 3);
	CHECK(Rgb[0] == 144);
	CHECK(Rgb[1] == 144);
	CHECK(Rgb[2] == 144);
}

TEST_CASE("An MDEC coefficient becomes its cosine wave", "[Graphics]")
{
	// The first AC coefficient in zigzag order is the lowest horizontal frequency. Level 1 at
	// scale 8 weighs (1 * 16 * 8 + 4) / 8 = 16; the orthonormal inverse DCT gives
	// 16 * sqrt(2) / 8 * cos((2x + 1) pi / 16) levels.
	std::vector<uint16> Codes = {Dc(8, 0), 0x0001, EndOfBlock};
	for (int32 Block = 1; Block < 6; ++Block)
	{
		Codes.push_back(Dc(8, 0));
		Codes.push_back(EndOfBlock);
	}
	const auto Picture = DecodeMdecPicture(Codes, 16, 16, FMdecTables::Standard());
	REQUIRE(Picture);
	// The Cr block comes first, so the wave is in the red chroma plane.
	for (int32 Y = 0; Y < 8; ++Y)
	{
		for (int32 X = 0; X < 8; ++X)
		{
			const double Expected = 16.0 * std::sqrt(2.0) / 8.0 * std::cos((2 * X + 1) * 3.14159265358979 / 16.0) * 16.0;
			CHECK(std::abs(Picture->ChromaRed[static_cast<std::size_t>(Y * 8 + X)] - Expected) <= 1.0);
		}
	}
}

TEST_CASE("MDEC macroblocks run down each column before the next", "[Graphics]")
{
	std::vector<uint16> Codes;
	AddFlatMacroblock(Codes, 8);
	AddFlatMacroblock(Codes, 16);
	const auto Tall = DecodeMdecPicture(Codes, 16, 32, FMdecTables::Standard());
	REQUIRE(Tall);
	CHECK(Tall->Luma[0] == 32);
	CHECK(Tall->Luma[16 * 16] == 64);
	const auto Wide = DecodeMdecPicture(Codes, 32, 16, FMdecTables::Standard());
	REQUIRE(Wide);
	CHECK(Wide->Luma[0] == 32);
	CHECK(Wide->Luma[16] == 64);
}

TEST_CASE("MDEC padding before a block is skipped, and a run past the end closes it", "[Graphics]")
{
	std::vector<uint16> Codes = {EndOfBlock, EndOfBlock, Dc(1, 64), static_cast<uint16>(63 << 10 | 5)};
	for (int32 Block = 1; Block < 6; ++Block)
	{
		Codes.push_back(Dc(1, 64));
		Codes.push_back(EndOfBlock);
	}
	const auto Picture = DecodeMdecPicture(Codes, 16, 16, FMdecTables::Standard());
	REQUIRE(Picture);
	CHECK(Picture->ChromaRed[0] == 256);
}

TEST_CASE("MDEC scale 0 stores values doubled, without the tables or the zigzag", "[Graphics]")
{
	// Doubled, the DC 32 is 64: flat 64 / 8 = 8 levels.
	std::vector<uint16> Codes;
	for (int32 Block = 0; Block < 6; ++Block)
	{
		Codes.push_back(Dc(0, 32));
		Codes.push_back(EndOfBlock);
	}
	const auto Picture = DecodeMdecPicture(Codes, 16, 16, FMdecTables::Standard());
	REQUIRE(Picture);
	CHECK(Picture->Luma[0] == 8 * 16);
}

TEST_CASE("An MDEC picture is refused when its codes run out", "[Graphics]")
{
	std::vector<uint16> Codes;
	AddFlatMacroblock(Codes, 1);
	CHECK_FALSE(DecodeMdecPicture(Codes, 32, 16, FMdecTables::Standard()));
	CHECK_FALSE(DecodeMdecPicture(Codes, 16, 20, FMdecTables::Standard()));
}
