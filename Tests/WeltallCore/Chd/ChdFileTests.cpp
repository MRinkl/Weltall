#include "Chd/ChdBytes.h"
#include "Chd/ChdFile.h"
#include "Chd/ChdImage.h"
#include "ChdFixtures.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <memory>

using namespace Chd;
using namespace Chd::Testing;

namespace
{
	TResult<FFile> Open(std::vector<uint8> Bytes)
	{
		return FFile::Open(std::make_shared<FMemoryInput>(std::move(Bytes)));
	}

	void WriteBigEndian(std::vector<uint8>& Bytes, std::size_t Offset, uint64 Value, std::size_t Size)
	{
		for (std::size_t Index = 0; Index < Size; ++Index)
		{
			Bytes[Offset + Index] = static_cast<uint8>(Value >> (8 * (Size - 1 - Index)));
		}
	}

	/** The first hunk of the kind, and its entry. */
	std::pair<uint32, FHunkEntry> FirstOf(const FFile& File, EHunkKind Kind)
	{
		const auto Found = std::find_if(File.Map().begin(), File.Map().end(), [Kind](const FHunkEntry& Entry) { return Entry.Kind() == Kind; });
		REQUIRE(Found != File.Map().end());
		return {static_cast<uint32>(Found - File.Map().begin()), *Found};
	}
}

TEST_CASE("Only a CHD of version 5 is opened", "[Chd]")
{
	const std::vector<uint8> Good = FromHex(LzmaFixture.Hex);
	REQUIRE(Open(Good));

	CHECK(Open(std::vector<uint8>(Good.begin(), Good.begin() + 10)).Error() == EError::NotChd);
	std::vector<uint8> Signature = Good;
	Signature[7] = 'X';
	CHECK(Open(Signature).Error() == EError::NotChd);

	for (const uint32 Version : {3u, 4u, 6u})
	{
		std::vector<uint8> Other = Good;
		WriteBigEndian(Other, 12, Version, 4);
		CHECK(Open(Other).Error() == EError::UnsupportedVersion);
	}

	std::vector<uint8> Length = Good;
	WriteBigEndian(Length, 8, 120, 4);
	CHECK(Open(Length).Error() == EError::Corrupt);
}

TEST_CASE("A CHD whose hunks cannot all be found is not opened", "[Chd]")
{
	const std::vector<uint8> Good = FromHex(LzmaFixture.Hex);

	std::vector<uint8> UnknownCodec = Good;
	WriteBigEndian(UnknownCodec, 20, MakeTag('a', 'b', 'c', 'd'), 4);
	CHECK(Open(UnknownCodec).Error() == EError::UnknownCodec);

	std::vector<uint8> Parent = Good;
	Parent[110] = 1;
	CHECK(Open(Parent).Error() == EError::NeedsParent);

	std::vector<uint8> NoHunkSize = Good;
	WriteBigEndian(NoHunkSize, 56, 0, 4);
	CHECK(Open(NoHunkSize).Error() == EError::Corrupt);

	std::vector<uint8> UnitsSplitHunks = Good;
	WriteBigEndian(UnitsSplitHunks, 60, 1000, 4);
	CHECK(Open(UnitsSplitHunks).Error() == EError::Corrupt);
}

TEST_CASE("A CHD cut short or with a damaged map is not opened", "[Chd]")
{
	const std::vector<uint8> Good = FromHex(LzmaFixture.Hex);
	const uint64 MapOffset = BigEndian::ReadU64(Good, 40);
	REQUIRE(MapOffset + 16 < Good.size());

	// chdman writes the map last: a copy still being made lacks it.
	CHECK(Open(std::vector<uint8>(Good.begin(), Good.end() - 40)).Error() == EError::CutShort);

	std::vector<uint8> FarMap = Good;
	WriteBigEndian(FarMap, 40, Good.size() + 1000, 8);
	CHECK(Open(FarMap).Error() == EError::CutShort);

	std::vector<uint8> MapCrc = Good;
	MapCrc[static_cast<std::size_t>(MapOffset) + 10] ^= 0x01;
	CHECK(Open(MapCrc).Error() == EError::Corrupt);

	std::vector<uint8> MapBits = Good;
	MapBits[static_cast<std::size_t>(MapOffset) + 20] ^= 0x10;
	CHECK_FALSE(Open(MapBits));
}

TEST_CASE("A damaged hunk does not unpack and neither does a copy of it", "[Chd]")
{
	const std::vector<uint8> Good = FromHex(LzmaFixture.Hex);
	const auto Intact = Open(Good);
	REQUIRE(Intact);
	FHunkDecoder Decoder;
	std::vector<uint8> Hunk(Intact->Header().HunkBytes);

	const auto [Packed, PackedEntry] = FirstOf(*Intact, EHunkKind::Codec0);
	const auto [Stored, StoredEntry] = FirstOf(*Intact, EHunkKind::Stored);
	const auto [Copy, CopyEntry] = FirstOf(*Intact, EHunkKind::Copy);
	REQUIRE(Intact->ReadHunk(Packed, Hunk, Decoder));
	REQUIRE(Intact->ReadHunk(Stored, Hunk, Decoder));
	REQUIRE(Intact->ReadHunk(Copy, Hunk, Decoder));
	CHECK_FALSE(Intact->ReadHunk(static_cast<uint32>(Intact->Map().size()), Hunk, Decoder));
	CHECK_FALSE(Intact->ReadHunk(Packed, std::span(Hunk).first(100), Decoder));

	std::vector<uint8> Damaged = Good;
	Damaged[static_cast<std::size_t>(PackedEntry.Offset() + PackedEntry.Length() / 2)] ^= 0x20;
	Damaged[static_cast<std::size_t>(StoredEntry.Offset()) + 7] ^= 0x01;
	std::vector<uint8> CopiedHunk = Good;
	const FHunkEntry& CopySource = Intact->Map()[CopyEntry.Source()];
	CopiedHunk[static_cast<std::size_t>(CopySource.Offset() + CopySource.Length() / 2)] ^= 0x04;

	const auto Broken = Open(Damaged);
	REQUIRE(Broken);
	CHECK_FALSE(Broken->ReadHunk(Packed, Hunk, Decoder));
	CHECK_FALSE(Broken->ReadHunk(Stored, Hunk, Decoder));
	const auto BrokenSource = Open(CopiedHunk);
	REQUIRE(BrokenSource);
	CHECK_FALSE(BrokenSource->ReadHunk(Copy, Hunk, Decoder));

	// A long read, whose hunks are unpacked on several threads, fails with any of them.
	const auto Image = FImage::Open(std::make_shared<FMemoryInput>(Damaged));
	REQUIRE(Image);
	std::vector<uint8> Whole(static_cast<std::size_t>((*Image)->Size()));
	CHECK_FALSE((*Image)->Read(0, Whole));
}
