#include "Graphics/VramStream.h"
#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

#include <initializer_list>

using namespace Weltall;
using Testing::FByteWriter;

namespace
{
	constexpr std::size_t SectorBytes = 2048;

	void AppendSector(std::vector<uint8>& Stream, std::vector<uint8> Sector)
	{
		Sector.resize(SectorBytes, 0);
		Stream.insert(Stream.end(), Sector.begin(), Sector.end());
	}

	/** A header sector, then one data sector per row count, sector k's units all equal to Fill + k. */
	void AppendBlock(std::vector<uint8>& Stream, uint32 Kind, std::initializer_list<int16> Place, int16 Width, std::initializer_list<int16> Rows, uint32 Blocks, uint16 Fill)
	{
		FByteWriter Header;
		Header.U32(Kind);
		for (const int16 Value : Place)
		{
			Header.S16(Value);
		}
		Header.S16(Width).S16(*Rows.begin()).S16(1).S16(static_cast<int16>(Rows.size())).U32(Blocks).U32(static_cast<uint32>(Rows.size()));
		for (const int16 Count : Rows)
		{
			Header.S16(Count);
		}
		AppendSector(Stream, Header.Bytes);
		uint16 Unit = Fill;
		for (const int16 Count : Rows)
		{
			FByteWriter Data;
			for (int32 Index = 0; Index < Width * Count; ++Index)
			{
				Data.U16(Unit);
			}
			AppendSector(Stream, Data.Bytes);
			++Unit;
		}
	}
}

TEST_CASE("A VRAM stream uploads each data sector below the rows its block filled so far", "[Graphics]")
{
	std::vector<uint8> Stream;
	AppendBlock(Stream, 0x1201, {0, 480, 0, 1}, 256, {4, 1}, 2, 0x10);
	// Later headers' block counts are not read; the first header's says the stream ends after this block.
	AppendBlock(Stream, 0x1200, {320, 0, 64, 8}, 64, {16}, 99, 0x20);
	AppendBlock(Stream, 0x1200, {0, 0, 0, 0}, 16, {1}, 1, 0x30);
	const auto Uploads = DecodeVramStream(Stream);
	REQUIRE(Uploads);
	REQUIRE(Uploads->size() == 3);
	const FVramImage& First = (*Uploads)[0];
	CHECK((First.Rect.X == 0 && First.Rect.Y == 481 && First.Rect.Width == 256 && First.Rect.Height == 4));
	CHECK(First.Units.size() == 256 * 4);
	CHECK(First.Units.front() == 0x10);
	const FVramImage& Second = (*Uploads)[1];
	CHECK((Second.Rect.Y == 485 && Second.Rect.Height == 1 && Second.Units.back() == 0x11));
	const FVramImage& Third = (*Uploads)[2];
	CHECK((Third.Rect.X == 384 && Third.Rect.Y == 8 && Third.Rect.Width == 64 && Third.Rect.Height == 16));
	CHECK(Third.Units[64 * 16 - 1] == 0x20);
}

TEST_CASE("A VRAM stream ends at a header of another kind or with its file", "[Graphics]")
{
	std::vector<uint8> Stream;
	AppendBlock(Stream, 0x1200, {320, 0, 0, 0}, 64, {8}, 5, 1);
	AppendSector(Stream, {0x02, 0x12});
	AppendBlock(Stream, 0x1200, {384, 0, 0, 0}, 64, {8}, 5, 2);
	auto Uploads = DecodeVramStream(Stream);
	REQUIRE(Uploads);
	CHECK(Uploads->size() == 1);

	std::vector<uint8> Cut;
	AppendBlock(Cut, 0x1200, {320, 0, 0, 0}, 64, {8, 8, 8}, 1, 1);
	Cut.resize(Cut.size() - SectorBytes);
	Uploads = DecodeVramStream(Cut);
	REQUIRE(Uploads);
	CHECK(Uploads->size() == 2);
}

TEST_CASE("A VRAM stream rejects sectors that cannot hold their rows", "[Graphics]")
{
	std::vector<uint8> Stream;
	AppendBlock(Stream, 0x1200, {320, 0, 0, 0}, 64, {17}, 1, 1);
	CHECK_FALSE(DecodeVramStream(Stream));

	std::vector<uint8> Wide;
	AppendBlock(Wide, 0x1200, {0, 0, 0, 0}, 0, {1}, 1, 1);
	CHECK_FALSE(DecodeVramStream(Wide));
}
