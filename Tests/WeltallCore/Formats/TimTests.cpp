#include "Formats/Tim.h"
#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;
using Testing::FByteWriter;

TEST_CASE("4-bit TIM with a CLUT", "[Formats]")
{
	FByteWriter File;
	File.U32(0x10).U32(0x08).U32(12 + 32).Rect(256, 480, 16, 1);
	for (uint16 Color = 0; Color < 16; ++Color)
	{
		File.U16(static_cast<uint16>(0x8000 | Color));
	}
	File.U32(12 + 4).Rect(832, 256, 1, 2).U8s({0x21, 0x43, 0x65, 0x87});

	const auto Tim = ParseTim(File.Bytes);
	REQUIRE(Tim);
	CHECK(Tim->BitsPerPixel == 4);
	REQUIRE(Tim->Clut.has_value());
	CHECK(Tim->Clut->Rect.Y == 480);
	CHECK(Tim->Clut->Units.size() == 16);
	CHECK(Tim->Clut->Units[3] == 0x8003);
	CHECK(Tim->Image.Units == std::vector<uint16>{0x4321, 0x8765});
	CHECK(Tim->PixelWidth() == 4);
	CHECK(Tim->IndexAt(0, 0) == 1);
	CHECK(Tim->IndexAt(1, 0) == 2);
	CHECK(Tim->IndexAt(3, 1) == 8);
	CHECK(Tim->ByteSize == File.Size());
}

TEST_CASE("8-bit TIM pixels hold two indices per unit", "[Formats]")
{
	FByteWriter File;
	File.U32(0x10).U32(0x01).U32(12 + 4).Rect(320, 0, 1, 2).U16(0x0201).U16(0xFF80);
	const auto Tim = ParseTim(File.Bytes);
	REQUIRE(Tim);
	CHECK_FALSE(Tim->Clut.has_value());
	CHECK(Tim->BitsPerPixel == 8);
	CHECK(Tim->IndexAt(0, 0) == 0x01);
	CHECK(Tim->IndexAt(1, 0) == 0x02);
	CHECK(Tim->IndexAt(0, 1) == 0x80);
	CHECK(Tim->IndexAt(1, 1) == 0xFF);
}

TEST_CASE("TIM parsing rejects truncated blocks and unknown pixel modes", "[Formats]")
{
	FByteWriter Truncated;
	Truncated.U32(0x10).U32(0x02).U32(12 + 64).Rect(0, 0, 4, 8);
	CHECK_FALSE(ParseTim(Truncated.Bytes));

	FByteWriter Mixed;
	Mixed.U32(0x10).U32(0x04).U32(12 + 2).Rect(0, 0, 1, 1).U16(0);
	CHECK_FALSE(ParseTim(Mixed.Bytes));

	CHECK_FALSE(ParseTim(std::vector<uint8>{0x11, 0, 0, 0, 0, 0, 0, 0}));
}

TEST_CASE("A TIM sequence gives each TIM's CLUT then its pixels until a word that is no TIM", "[Formats]")
{
	FByteWriter Bytes;
	Bytes.U32(0x10).U32(0x08).U32(12 + 4).Rect(0, 480, 2, 1).U16(0x1111).U16(0x2222);
	Bytes.U32(12 + 4).Rect(320, 0, 1, 1).U16(0x3333).U16(0);
	// A pixel mode ParseTim rejects is nothing to ReadTIM, which steps over a block by its length in
	// whole words: past this 15-byte block after 12 bytes, so its one pixel is the next TIM's first unit.
	Bytes.U32(0x10).U32(0x07).U32(12 + 3).Rect(384, 8, 1, 1);
	Bytes.U32(0x10).U32(0x02).U32(12 + 4).Rect(448, 0, 1, 1).U16(0x5555).U16(0);
	Bytes.U32(0x11).U32(0x00);
	const auto Images = ReadTimSequence(Bytes.Bytes);
	REQUIRE(Images);
	REQUIRE(Images->size() == 4);
	CHECK((*Images)[0].Rect.Y == 480);
	CHECK((*Images)[0].Units == std::vector<uint16>{0x1111, 0x2222});
	CHECK((*Images)[1].Rect.X == 320);
	CHECK((*Images)[1].Units == std::vector<uint16>{0x3333});
	CHECK((*Images)[2].Rect.X == 384);
	CHECK((*Images)[2].Units == std::vector<uint16>{0x10});
	CHECK((*Images)[3].Units == std::vector<uint16>{0x5555});

	FByteWriter Truncated;
	Truncated.U32(0x10).U32(0x02).U32(12 + 64).Rect(0, 0, 4, 8);
	CHECK_FALSE(ReadTimSequence(Truncated.Bytes));
	const auto Empty = ReadTimSequence(std::vector<uint8>{});
	REQUIRE(Empty);
	CHECK(Empty->empty());
}
