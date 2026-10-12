#include "Formats/Lzss.h"
#include "Foundation/Sha1.h"
#include "Media/GameFileIndex.h"
#include "Support/ByteWriter.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <string>

using namespace Weltall;
using Testing::FByteWriter;

namespace
{
	std::string AsText(const std::vector<uint8>& Bytes)
	{
		return std::string(Bytes.begin(), Bytes.end());
	}
}

TEST_CASE("LZSS copies literals and back-references that overlap their own output", "[Formats]")
{
	// Four literals, then 1 + 3 = 4 bytes from 2 back ("cdcd"), then three more literals.
	FByteWriter Stream;
	Stream.U32(11).U8(0b00010000).U8s({'a', 'b', 'c', 'd', 2, 0x10, 'x', 'y', 'z'});
	const auto Output = UnpackLzss(Stream.Bytes);
	REQUIRE(Output);
	CHECK(AsText(*Output) == "abcdcdcdxyz");
}

TEST_CASE("LZSS distances take their high bits from the second byte", "[Formats]")
{
	// 256 literal bytes 0-255, then 2 + 3 = 5 bytes from 0x100 back, then seven literals.
	FByteWriter Stream;
	Stream.U32(256 + 5 + 7);
	for (int32 Group = 0; Group < 32; ++Group)
	{
		Stream.U8(0);
		for (int32 Byte = 0; Byte < 8; ++Byte)
		{
			Stream.U8(static_cast<uint8>(Group * 8 + Byte));
		}
	}
	Stream.U8(0b00000001).U8s({0x00, 0x21, '1', '2', '3', '4', '5', '6', '7'});
	const auto Output = UnpackLzss(Stream.Bytes);
	REQUIRE(Output);
	REQUIRE(Output->size() == 268);
	CHECK(std::vector<uint8>(Output->begin() + 256, Output->begin() + 261) == std::vector<uint8>{0, 1, 2, 3, 4});
	CHECK(AsText(std::vector<uint8>(Output->begin() + 261, Output->end())) == "1234567");
}

TEST_CASE("LZSS rejects streams the game's data never contains", "[Formats]")
{
	FByteWriter Truncated;
	Truncated.U32(8).U8(0).U8s({'a', 'b', 'c'});
	CHECK_FALSE(UnpackLzss(Truncated.Bytes));

	FByteWriter Overshoot;
	Overshoot.U32(5).U8(0).U8s({'1', '2', '3', '4', '5', '6', '7', '8'});
	CHECK_FALSE(UnpackLzss(Overshoot.Bytes));

	FByteWriter BeforeStart;
	BeforeStart.U32(10).U8(0b00000001).U8s({0x05, 0x00, 'A', 'B', 'C', 'D', 'E', 'F', 'G'});
	CHECK_FALSE(UnpackLzss(BeforeStart.Bytes));

	FByteWriter ZeroDistance;
	ZeroDistance.U32(11).U8(0b00000010).U8s({'A', 0x00, 0x00, 'B', 'C', 'D', 'E', 'F', 'G'});
	CHECK_FALSE(UnpackLzss(ZeroDistance.Bytes));

	FByteWriter Huge;
	Huge.U32(0x01000000).U8(0);
	CHECK_FALSE(UnpackLzss(Huge.Bytes));
	CHECK_FALSE(UnpackLzss(std::vector<uint8>{1, 0}));
}

TEST_CASE("An LZSS prefix stops inside a group and a reference without reading further", "[Formats]")
{
	// 11 bytes in two groups, the second group's reference cut short and followed by nothing.
	FByteWriter Stream;
	Stream.U32(11).U8(0b00010000).U8s({'a', 'b', 'c', 'd', 2, 0x10, 'x', 'y', 'z'});
	const auto Prefix = UnpackLzssPrefix(Stream.Bytes, 6);
	REQUIRE(Prefix);
	CHECK(AsText(*Prefix) == "abcdcd");

	FByteWriter Cut;
	Cut.U32(12).U8(0).U8s({'a', 'b', 'c'});
	const auto Short = UnpackLzssPrefix(Cut.Bytes, 3);
	REQUIRE(Short);
	CHECK(AsText(*Short) == "abc");
	CHECK_FALSE(UnpackLzssPrefix(Cut.Bytes, 4));
	CHECK_FALSE(UnpackLzssPrefix(Cut.Bytes, 13));
	const auto Nothing = UnpackLzssPrefix(Cut.Bytes, 0);
	REQUIRE(Nothing);
	CHECK(Nothing->empty());
}

TEST_CASE("The game's programs unpack from their files as the game unpacks them", "[Formats][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	struct FProgramFile
	{
		int32 Number;
		std::size_t Size;
		const char* Sha1;
	};
	// Files of directory 1; the digests come from the game's own decompressor run on each file.
	const FProgramFile Programs[] = {
		{14, 0x3FAFE, "7bdc0e2be7797ab821fb7d0ce10438a0e91f4c7b"},
		{16, 0x53F80, "124703d415f570f9cd8d7dad89c6a4eddfd7e6f0"},
		{15, 0x2C0C6, "5fee1deab3558c23bbe9c3a2eda3a2021a570911"},
		{13, 0x22E69, "9cfebaf201a0dfd352c11007a63ad7a955533b17"},
		{17, 0x25908, "fd911d26b921d6eb0c73c3bbc83ea82dc459122f"},
		{18, 0x7453, "e7ff2568b918b75e290050597c86584f3499ebdf"},
	};
	for (const FProgramFile& Program : Programs)
	{
		INFO("file 1/" << Program.Number);
		const FIndexRecord* Record = Index->Find(1, Program.Number);
		REQUIRE(Record != nullptr);
		const auto Sectors = ReadFileSectors(Disc->Image(), *Record);
		REQUIRE(Sectors);
		const auto Unpacked = UnpackLzss(*Sectors);
		REQUIRE(Unpacked);
		CHECK(Unpacked->size() == Program.Size);
		CHECK(FSha1::ToHex(FSha1::Hash(*Unpacked)) == Program.Sha1);
	}
}
