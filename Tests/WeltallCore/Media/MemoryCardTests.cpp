#include "Media/MemoryCard.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <span>
#include <string>

using namespace Weltall;

namespace
{
	constexpr uint8 FirstBlockInUse = 0x51;
	constexpr uint8 LastBlockInUse = 0x53;
	constexpr uint8 Deleted = 0xA1;

	void WriteU32(std::vector<uint8>& Bytes, std::size_t Offset, uint32 Value)
	{
		std::memcpy(Bytes.data() + Offset, &Value, 4);
	}

	void WriteU16(std::vector<uint8>& Bytes, std::size_t Offset, uint16 Value)
	{
		std::memcpy(Bytes.data() + Offset, &Value, 2);
	}

	void SetEntry(std::vector<uint8>& Card, int32 Block, uint8 State, uint32 Size, uint16 Next, const char* Name)
	{
		const std::size_t Frame = static_cast<std::size_t>(Block) * MemoryCard::FrameSize;
		WriteU32(Card, Frame, State);
		WriteU32(Card, Frame + 4, Size);
		WriteU16(Card, Frame + 8, Next);
		std::memcpy(Card.data() + Frame + 0x0A, Name, std::strlen(Name));
	}

	/** Block n starts "SC" and is filled with the byte n after it. */
	void FillBlock(std::vector<uint8>& Card, int32 Block)
	{
		const std::size_t Start = static_cast<std::size_t>(Block) * MemoryCard::BlockSize;
		std::memset(Card.data() + Start, Block, MemoryCard::BlockSize);
		Card[Start] = 'S';
		Card[Start + 1] = 'C';
	}

	/**
	 * A card with a one-block save in block 1, a two-block save whose blocks are 2 and 5, and a
	 * deleted one in block 3.
	 */
	std::vector<uint8> MakeCard()
	{
		std::vector<uint8> Card(MemoryCard::CardSize, 0);
		Card[0] = 'M';
		Card[1] = 'C';
		SetEntry(Card, 1, FirstBlockInUse, 0x2000, 0xFFFF, "BASLUS-006640");
		SetEntry(Card, 2, FirstBlockInUse, 0x4000, 4, "BASLUS-006641");
		SetEntry(Card, 5, LastBlockInUse, 0, 0xFFFF, "");
		SetEntry(Card, 3, Deleted, 0x2000, 0xFFFF, "BASLUS-006642");
		for (const int32 Block : {1, 2, 3, 5})
		{
			FillBlock(Card, Block);
		}
		return Card;
	}

	void CheckCardSaves(const TResult<std::vector<FCardSave>>& Saves)
	{
		REQUIRE(Saves);
		REQUIRE(Saves->size() == 2);
		CHECK((*Saves)[0].FileName == "BASLUS-006640");
		CHECK((*Saves)[0].Blocks.size() == MemoryCard::BlockSize);
		CHECK((*Saves)[0].Blocks[2] == 1);
		CHECK((*Saves)[1].FileName == "BASLUS-006641");
		REQUIRE((*Saves)[1].Blocks.size() == 2 * MemoryCard::BlockSize);
		CHECK((*Saves)[1].Blocks[2] == 2);
		CHECK((*Saves)[1].Blocks[MemoryCard::BlockSize + 2] == 5);
	}

	std::vector<uint8> WithHeader(const std::string& Magic, std::size_t HeaderBytes, const std::vector<uint8>& Card)
	{
		std::vector<uint8> File(HeaderBytes, 0);
		std::memcpy(File.data(), Magic.data(), Magic.size());
		File.insert(File.end(), Card.begin(), Card.end());
		return File;
	}
}

TEST_CASE("A raw memory card's saves are followed through the directory's links", "[Media]")
{
	CheckCardSaves(ReadMemoryCardImage(MakeCard()));
}

TEST_CASE("Memory cards behind DexDrive, Virtual Game Station and PSV headers read the same", "[Media]")
{
	const std::vector<uint8> Card = MakeCard();
	CheckCardSaves(ReadMemoryCardImage(WithHeader("123-456-STD", 0xF40, Card)));
	// DexDrive images whose header was zeroed, signature and all.
	CheckCardSaves(ReadMemoryCardImage(WithHeader("", 0xF40, Card)));
	CheckCardSaves(ReadMemoryCardImage(WithHeader("VgsM", 0x40, Card)));
	CheckCardSaves(ReadMemoryCardImage(WithHeader("PSV", 0x100, Card)));
}

TEST_CASE("A memory card image may leave out its unused blocks at the end", "[Media]")
{
	std::vector<uint8> Card = MakeCard();
	Card.resize(6 * MemoryCard::BlockSize);
	CheckCardSaves(ReadMemoryCardImage(Card));
}

TEST_CASE("Single saves come as MCS or Xploder files", "[Media]")
{
	const std::vector<uint8> Card = MakeCard();
	const std::vector<uint8> Blocks(Card.begin() + MemoryCard::BlockSize, Card.begin() + 2 * MemoryCard::BlockSize);

	std::vector<uint8> Mcs(Card.begin() + MemoryCard::FrameSize, Card.begin() + 2 * MemoryCard::FrameSize);
	Mcs.insert(Mcs.end(), Blocks.begin(), Blocks.end());
	const auto FromMcs = ReadMemoryCardImage(Mcs);
	REQUIRE(FromMcs);
	REQUIRE(FromMcs->size() == 1);
	CHECK((*FromMcs)[0].FileName == "BASLUS-006640");
	CHECK((*FromMcs)[0].Blocks == Blocks);

	std::vector<uint8> Xploder = WithHeader("BASLUS-006640", 0x36, Blocks);
	const auto FromXploder = ReadMemoryCardImage(Xploder);
	REQUIRE(FromXploder);
	REQUIRE(FromXploder->size() == 1);
	CHECK((*FromXploder)[0].FileName == "BASLUS-006640");
	CHECK((*FromXploder)[0].Blocks == Blocks);
}

TEST_CASE("A file that is no memory card is refused", "[Media]")
{
	CHECK_FALSE(ReadMemoryCardImage(std::vector<uint8>(1000, 0)));
	std::vector<uint8> Card = MakeCard();
	Card[0] = 'X';
	CHECK_FALSE(ReadMemoryCardImage(Card));
}

TEST_CASE("A written memory card is a formatted card whose saves read back", "[Media]")
{
	FCardSave One{"BASLUS-006640", std::vector<uint8>(MemoryCard::BlockSize, 7)};
	FCardSave Two{"BASLUS-00664>", std::vector<uint8>(2 * MemoryCard::BlockSize, 9)};
	for (FCardSave* Save : {&One, &Two})
	{
		Save->Blocks[0] = 'S';
		Save->Blocks[1] = 'C';
	}
	Two.Blocks[MemoryCard::BlockSize] = 10;
	const std::array Saves{One, Two};
	const auto Card = WriteMemoryCardImage(Saves);
	REQUIRE(Card);
	REQUIRE(Card->size() == MemoryCard::CardSize);
	const auto Frame = [&Card](int32 Number) { return std::span<const uint8>(*Card).subspan(static_cast<std::size_t>(Number) * MemoryCard::FrameSize, MemoryCard::FrameSize); };

	// The header and its copy for the write test; every frame with a checksum ends with the exclusive-or of its bytes.
	CHECK(Frame(0)[0] == 'M');
	CHECK(Frame(0)[1] == 'C');
	CHECK(Frame(0)[0x7F] == 0x0E);
	CHECK(std::equal(Frame(0).begin(), Frame(0).end(), Frame(63).begin()));
	for (int32 Number = 0; Number < 36; ++Number)
	{
		uint8 Sum = 0;
		for (const uint8 Byte : Frame(Number))
		{
			Sum ^= Byte;
		}
		INFO(Number);
		CHECK(Sum == 0);
	}
	// The directory: the first save's block, the second's two linked, then free entries.
	CHECK(Frame(1)[0] == FirstBlockInUse);
	CHECK(Frame(1)[5] == 0x20);
	CHECK(Frame(1)[8] == 0xFF);
	CHECK(Frame(2)[0] == FirstBlockInUse);
	CHECK(Frame(2)[5] == 0x40);
	CHECK(Frame(2)[8] == 2);
	CHECK(Frame(2)[9] == 0);
	CHECK(Frame(3)[0] == LastBlockInUse);
	CHECK(Frame(3)[8] == 0xFF);
	CHECK(Frame(3)[0x0A] == 0);
	for (int32 Free = 4; Free <= 15; ++Free)
	{
		CHECK(Frame(Free)[0] == 0xA0);
		CHECK(Frame(Free)[8] == 0xFF);
		CHECK(Frame(Free)[9] == 0xFF);
		CHECK(Frame(Free)[0x7F] == 0xA0);
	}
	// No broken sectors, and the unused frames as formatting leaves them.
	for (int32 Broken = 16; Broken < 36; ++Broken)
	{
		CHECK(Frame(Broken)[0] == 0xFF);
		CHECK(Frame(Broken)[3] == 0xFF);
		CHECK(Frame(Broken)[4] == 0);
		CHECK(Frame(Broken)[8] == 0xFF);
		CHECK(Frame(Broken)[0x7F] == 0);
	}
	CHECK(Frame(36)[0] == 0xFF);
	CHECK(Frame(62)[0x7F] == 0xFF);

	const auto Read = ReadMemoryCardImage(*Card);
	REQUIRE(Read);
	REQUIRE(Read->size() == 2);
	CHECK((*Read)[0].FileName == One.FileName);
	CHECK((*Read)[0].Blocks == One.Blocks);
	CHECK((*Read)[1].FileName == Two.FileName);
	CHECK((*Read)[1].Blocks == Two.Blocks);

	// An empty card is a card too.
	const auto Empty = WriteMemoryCardImage({});
	REQUIRE(Empty);
	const auto None = ReadMemoryCardImage(*Empty);
	REQUIRE(None);
	CHECK(None->empty());
}

TEST_CASE("A memory card takes whole blocks, 15 at most, under names its directory can hold", "[Media]")
{
	const std::vector<uint8> Block(MemoryCard::BlockSize, 1);
	CHECK(WriteMemoryCardImage(std::vector<FCardSave>(15, FCardSave{"A", Block})));
	CHECK_FALSE(WriteMemoryCardImage(std::vector<FCardSave>(16, FCardSave{"A", Block})));
	CHECK_FALSE(WriteMemoryCardImage(std::array{FCardSave{"A", std::vector<uint8>(100, 1)}}));
	CHECK_FALSE(WriteMemoryCardImage(std::array{FCardSave{"A", {}}}));
	CHECK(WriteMemoryCardImage(std::array{FCardSave{std::string(20, 'N'), Block}}));
	CHECK_FALSE(WriteMemoryCardImage(std::array{FCardSave{std::string(21, 'N'), Block}}));
}
