#include "Game/OriginalSave.h"
#include "Media/MemoryCard.h"

#include <catch_amalgamated.hpp>

#include <numeric>
#include <vector>

using namespace Weltall;

namespace
{
	/** A one-block save with a party of Fei and Elly at levels 5 and 3, story progress 0x21 and a checksum that holds. */
	FCardSave MakeSave(const char* Name = "BASLUS-006643")
	{
		FCardSave Save;
		Save.FileName = Name;
		Save.Blocks.assign(0x2000, 0);
		uint8* Data = Save.Blocks.data() + FOriginalSave::DataOffset;
		Data[0] = 0x10;
		Data[1] = 0x0E;
		Data[0x16] = 5;
		Data[0x17] = 3;
		Data[0x1C] = 0;
		Data[0x1D] = 1;
		Data[0x1E] = FOriginalSave::EmptySlot;
		Data[0x23] = 3;
		Data[0x1134] = 0x21;
		Data[FOriginalSave::DataSize - 1] = static_cast<uint8>(std::accumulate(Data, Data + FOriginalSave::DataSize - 1, 0u));
		return Save;
	}
}

TEST_CASE("An original save keeps the game's data and reads its summary", "[Game]")
{
	const auto Save = FOriginalSave::FromCard(MakeSave());
	REQUIRE(Save);
	CHECK(Save->Data().size() == FOriginalSave::DataSize);
	CHECK(Save->PlayTime() == 0x0E10);
	CHECK(Save->StoryProgress() == 0x21);
	CHECK(Save->Party() == std::array<uint8, 3>{0, 1, FOriginalSave::EmptySlot});
	CHECK(Save->Levels() == std::array<uint8, 3>{5, 3, 0});
	CHECK(Save->CardSlot() == 3);
}

TEST_CASE("An original save is refused when its checksum fails or it is another game's", "[Game]")
{
	FCardSave Damaged = MakeSave();
	Damaged.Blocks[FOriginalSave::DataOffset + 0x100] ^= 1;
	CHECK_FALSE(FOriginalSave::FromCard(Damaged));
	CHECK_FALSE(FOriginalSave::FromCard(MakeSave("BASLUS-012345")));
}

TEST_CASE("A save's data is refused unless whole with its checksum, which the writer computes", "[Game]")
{
	std::vector<uint8> Data(FOriginalSave::DataSize, 0);
	Data[0x10] = 0x80;
	Data[0x20] = 0x81;
	CHECK_FALSE(FOriginalSave::FromData(Data));
	CHECK_FALSE(FOriginalSave::FromData(std::span<const uint8>(Data).first(FOriginalSave::DataSize - 1)));
	const FOriginalSave Save = FOriginalSave::WithChecksum(Data);
	CHECK(Save.Data().back() == 0x01);
	CHECK(FOriginalSave::FromData(Save.Data()));
}
