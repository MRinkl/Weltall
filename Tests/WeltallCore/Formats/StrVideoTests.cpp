#include "Formats/StrVideo.h"
#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;
using Testing::FByteWriter;

namespace
{
	/** A video sector's user data whose payload bytes are all Fill. */
	std::vector<uint8> MakeSector(uint16 Index, uint16 Count, uint32 Frame, uint32 FrameBytes, uint8 Fill)
	{
		FByteWriter Sector;
		Sector.U16(0x0160).U16(0x8001).U16(Index).U16(Count).U32(Frame).U32(FrameBytes).U16(320).U16(224);
		Sector.Bytes.resize(StrSector::HeaderBytes, 0);
		Sector.Bytes.resize(StrSector::HeaderBytes + StrSector::PayloadBytes, Fill);
		return Sector.Bytes;
	}
}

TEST_CASE("A movie's video sector names its place in its frame", "[Formats]")
{
	const auto Header = ParseStrSector(MakeSector(1, 9, 42, 12932, 0));
	REQUIRE(Header);
	CHECK(Header->SectorIndex == 1);
	CHECK(Header->SectorCount == 9);
	CHECK(Header->FrameNumber == 42);
	CHECK(Header->FrameBytes == 12932);
	CHECK(Header->Width == 320);
	CHECK(Header->Height == 224);

	std::vector<uint8> NotVideo = MakeSector(0, 1, 1, 100, 0);
	NotVideo[2] = 0;
	CHECK_FALSE(ParseStrSector(NotVideo));
	CHECK_FALSE(ParseStrSector(MakeSector(3, 3, 1, 100, 0)));
}

TEST_CASE("Video sectors join into a frame cut to its recorded size", "[Formats]")
{
	FStrFrameAssembler Assembler;
	CHECK_FALSE(Assembler.Add(MakeSector(0, 2, 7, StrSector::PayloadBytes + 10, 0xAA)));
	const auto Frame = Assembler.Add(MakeSector(1, 2, 7, StrSector::PayloadBytes + 10, 0xBB));
	REQUIRE(Frame);
	CHECK(Frame->FrameNumber == 7);
	REQUIRE(Frame->Bitstream.size() == StrSector::PayloadBytes + 10);
	CHECK(Frame->Bitstream.front() == 0xAA);
	CHECK(Frame->Bitstream.back() == 0xBB);
}

TEST_CASE("A frame missing a sector is dropped and the next one still comes through", "[Formats]")
{
	FStrFrameAssembler Assembler;
	CHECK_FALSE(Assembler.Add(MakeSector(0, 3, 1, 100, 0)));
	CHECK_FALSE(Assembler.Add(MakeSector(2, 3, 1, 100, 0)));
	CHECK_FALSE(Assembler.Add(MakeSector(0, 2, 2, 100, 0)));
	CHECK_FALSE(Assembler.Add(MakeSector(1, 2, 3, 100, 0)));
	const auto Next = Assembler.Add(MakeSector(0, 1, 4, 100, 0));
	REQUIRE(Next);
	CHECK(Next->FrameNumber == 4);
}
