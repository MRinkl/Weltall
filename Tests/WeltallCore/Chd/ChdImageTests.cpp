#include "Chd/ChdImage.h"
#include "ChdFixtures.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <memory>

using namespace Chd;
using namespace Chd::Testing;

namespace
{
	std::unique_ptr<FImage> OpenFixture(const FFixture& Fixture)
	{
		auto Image = FImage::Open(std::make_shared<FMemoryInput>(FromHex(Fixture.Hex)));
		REQUIRE(Image);
		return std::move(*Image);
	}

	bool HasKind(const FFile& File, EHunkKind Kind)
	{
		return std::any_of(File.Map().begin(), File.Map().end(), [Kind](const FHunkEntry& Entry) { return Entry.Kind() == Kind; });
	}
}

TEST_CASE("Each CHD unpacks to the image chdman packed", "[Chd]")
{
	for (const FFixture* Fixture : {&LzmaFixture, &ZlibFixture, &HuffmanFixture, &ZstdFixture, &FlacFixture, &MixedFixture, &PlainFixture})
	{
		INFO(Fixture->Name);
		const auto Image = OpenFixture(*Fixture);
		REQUIRE(Image->Size() == Fixture->ImageBytes);
		std::vector<uint8> Bytes(static_cast<std::size_t>(Image->Size()));
		REQUIRE(Image->Read(0, Bytes));
		CHECK(Fnv1a(Bytes) == Fixture->ImageHash);
	}
}

TEST_CASE("A CHD's header names its codecs and the sizes of its hunks and units", "[Chd]")
{
	const auto Lzma = OpenFixture(LzmaFixture);
	const FHeader& Header = Lzma->File().Header();
	CHECK(Header.HunkBytes == 2048);
	CHECK(Header.UnitBytes == 2048);
	CHECK(Header.Codecs == std::array<uint32, 4>{Codec::Lzma, Codec::None, Codec::None, Codec::None});
	CHECK(Header.HunkCount() == LzmaFixture.ImageBytes / 2048);
	CHECK(Lzma->File().FindMetadata(MetadataTag::Dvd) != nullptr);
	CHECK(Lzma->File().FindMetadata(MetadataTag::CdTrack) == nullptr);

	const auto Mixed = OpenFixture(MixedFixture);
	CHECK(Mixed->File().Header().HunkBytes == 4096);
	CHECK(Mixed->File().Header().Codecs == std::array<uint32, 4>{Codec::Lzma, Codec::Zlib, Codec::Huffman, Codec::Flac});
}

TEST_CASE("A CHD's map tells packed, stored, copied and missing hunks apart", "[Chd]")
{
	for (const FFixture* Fixture : {&LzmaFixture, &ZlibFixture, &HuffmanFixture, &ZstdFixture})
	{
		INFO(Fixture->Name);
		const auto Image = OpenFixture(*Fixture);
		CHECK(HasKind(Image->File(), EHunkKind::Codec0));
		CHECK(HasKind(Image->File(), EHunkKind::Stored));
		CHECK(HasKind(Image->File(), EHunkKind::Copy));
	}
	const auto Flac = OpenFixture(FlacFixture);
	CHECK(HasKind(Flac->File(), EHunkKind::Codec0));
	// FLAC is the default's fourth codec.
	const auto Mixed = OpenFixture(MixedFixture);
	CHECK(HasKind(Mixed->File(), EHunkKind::Codec3));

	const auto Plain = OpenFixture(PlainFixture);
	CHECK(HasKind(Plain->File(), EHunkKind::Plain));
	CHECK(HasKind(Plain->File(), EHunkKind::Zeros));
	CHECK_FALSE(HasKind(Plain->File(), EHunkKind::Codec0));
}

TEST_CASE("Small reads across hunks give the bytes of one whole read", "[Chd]")
{
	const auto Image = OpenFixture(MixedFixture);
	std::vector<uint8> Whole(static_cast<std::size_t>(Image->Size()));
	REQUIRE(Image->Read(0, Whole));

	// Pieces of a prime size fall across every hunk boundary in turn, and some lie wholly within a hunk.
	std::vector<uint8> Pieces(Whole.size());
	constexpr std::size_t Piece = 1531;
	for (std::size_t At = 0; At < Pieces.size(); At += Piece)
	{
		const std::size_t Size = std::min(Piece, Pieces.size() - At);
		REQUIRE(Image->Read(At, std::span(Pieces).subspan(At, Size)));
	}
	CHECK(Pieces == Whole);

	// Read backwards, so that no piece finds its hunk kept from the read before.
	std::vector<uint8> Backwards(Whole.size());
	for (std::size_t End = Backwards.size(); End > 0;)
	{
		const std::size_t Size = std::min<std::size_t>(5000, End);
		End -= Size;
		REQUIRE(Image->Read(End, std::span(Backwards).subspan(End, Size)));
	}
	CHECK(Backwards == Whole);
}

TEST_CASE("Reads past the image's end are refused", "[Chd]")
{
	const auto Image = OpenFixture(LzmaFixture);
	std::array<uint8, 16> Bytes{};
	CHECK(Image->Read(Image->Size() - Bytes.size(), Bytes));
	CHECK_FALSE(Image->Read(Image->Size() - Bytes.size() + 1, Bytes));
	CHECK_FALSE(Image->Read(Image->Size() + 1, std::span(Bytes).first(0)));
}
