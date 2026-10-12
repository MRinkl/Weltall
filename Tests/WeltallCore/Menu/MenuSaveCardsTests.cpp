#include "Menu/MenuSaveCards.h"
#include "Game/GameState.h"
#include "Game/SaveStore.h"
#include "Game/SaveWrite.h"
#include "Media/GameFileIndex.h"
#include "Media/MemoryCard.h"
#include "Media/PsxExecutable.h"
#include "Menu/MenuResources.h"
#include "Menu/MenuSaveHeader.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <filesystem>

using namespace Weltall;

// The player's save stores as memory card images, and the game's saves read from such images.
namespace
{
	constexpr int32 SecondStore = 1;
	/** The second store's third and last slots. */
	constexpr int32 ThirdSlot = 2;
	constexpr int32 LastSlot = 14;

	struct FScratchStore
	{
		std::filesystem::path Path = std::filesystem::temp_directory_path() / "WeltallMenuSaveCards";
		FSaveStore Store{Path};

		FScratchStore()
		{
			std::error_code Error;
			std::filesystem::remove_all(Path, Error);
		}

		~FScratchStore()
		{
			std::error_code Error;
			std::filesystem::remove_all(Path, Error);
		}
	};
}

TEST_CASE("A save store becomes a memory card of the game's own files, which read back as its saves", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Resources = FMenuResources::Load(Disc->Image(), *Index, *Executable);
	REQUIRE(Resources);
	auto NewGame = LoadNewGameState(Disc->Image(), *Index, Resident.NameTables);
	REQUIRE(NewGame);

	FScratchStore Scratch;
	FSaveStore& Store = Scratch.Store;
	const FOriginalSave Third = WriteOriginalSave(*NewGame, ThirdSlot, 0, Resident.NameTables, Resident.Font.LeadByte());
	NewGame->SetGold(4321);
	const FOriginalSave Last = WriteOriginalSave(*NewGame, LastSlot, 1, Resident.NameTables, Resident.Font.LeadByte());
	// The last slot first: the card lists a store's saves by slot.
	REQUIRE_FALSE(Store.Write(SecondStore * FSaveStore::SlotsPerStore + LastSlot, Last));
	REQUIRE_FALSE(Store.Write(SecondStore * FSaveStore::SlotsPerStore + ThirdSlot, Third));
	// Another store's save stays out of the card.
	REQUIRE_FALSE(Store.Write(0, Third));

	const auto Card = ExportStoreCard(Store, SecondStore, *Resources);
	REQUIRE(Card);
	CHECK(Card->size() == MemoryCard::CardSize);
	const auto Files = ReadMemoryCardImage(*Card);
	REQUIRE(Files);
	REQUIRE(Files->size() == 2);
	// Slot 2's file is named with '2', slot 14's with '>', each a block of the game's header and the save's data.
	CHECK((*Files)[0].FileName == "BASLUS-006642");
	CHECK((*Files)[1].FileName == "BASLUS-00664>");
	const std::array<const FOriginalSave*, 2> Saved{&Third, &Last};
	const std::array<int32, 2> Slots{ThirdSlot, LastSlot};
	for (std::size_t File = 0; File < 2; ++File)
	{
		const std::vector<uint8>& Blocks = (*Files)[File].Blocks;
		REQUIRE(Blocks.size() == MemoryCard::BlockSize);
		const auto Header = MenuSaveHeader(*Resources, Slots[File], Saved[File]->Data());
		CHECK(std::equal(Header.begin(), Header.end(), Blocks.begin()));
		CHECK(std::equal(Saved[File]->Data().begin(), Saved[File]->Data().end(), Blocks.begin() + SaveHeaderSize));
	}

	const auto Saves = ReadCardImageSaves(*Card);
	REQUIRE(Saves);
	REQUIRE(Saves->size() == 2);
	CHECK((*Saves)[0].CardSlot() == ThirdSlot);
	CHECK((*Saves)[1].CardSlot() == LastSlot);
	CHECK(std::ranges::equal((*Saves)[1].Data(), Last.Data()));

	// An empty store is an empty card; a store that is none, an error.
	const auto Empty = ExportStoreCard(Store, 5, *Resources);
	REQUIRE(Empty);
	const auto None = ReadCardImageSaves(*Empty);
	REQUIRE(None);
	CHECK(None->empty());
	CHECK_FALSE(ExportStoreCard(Store, FSaveStore::StoreCount, *Resources));
}

TEST_CASE("Only the game's sound saves are read from a card image", "[Menu]")
{
	std::vector<uint8> Data(FOriginalSave::DataSize, 0);
	Data[0] = 5;
	const FOriginalSave Sound = FOriginalSave::WithChecksum(Data);
	const auto Block = [](std::span<const uint8> Bytes) {
		std::vector<uint8> Blocks(MemoryCard::BlockSize, 0);
		Blocks[0] = 'S';
		Blocks[1] = 'C';
		std::copy(Bytes.begin(), Bytes.end(), Blocks.begin() + FOriginalSave::DataOffset);
		return Blocks;
	};
	std::vector<uint8> Damaged(Sound.Data().begin(), Sound.Data().end());
	Damaged[100] ^= 1;
	const std::array Files{FCardSave{"BASLUS-006643", Block(Damaged)}, FCardSave{"BASLUS-012345", Block(Sound.Data())},
		FCardSave{"BASLUS-006640", Block(Sound.Data())}};
	const auto Card = WriteMemoryCardImage(Files);
	REQUIRE(Card);
	const auto Saves = ReadCardImageSaves(*Card);
	REQUIRE(Saves);
	REQUIRE(Saves->size() == 1);
	CHECK(std::ranges::equal((*Saves)[0].Data(), Sound.Data()));
	CHECK_FALSE(ReadCardImageSaves(std::vector<uint8>(500, 0)));
}

TEST_CASE("The game's icon is its save icon, its first colour clear", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Icon = ReadGameIcon(Disc->Image(), *Index);
	REQUIRE(Icon);
	const auto Resources = FMenuResources::Load(Disc->Image(), *Index, *Executable);
	REQUIRE(Resources);
	// The same pixels the saves carry; the red X's strokes the reddest of them.
	int32 Red = 0;
	for (std::size_t Pixel = 0; Pixel < Icon->Pixels.size(); ++Pixel)
	{
		const uint8 Entry = static_cast<uint8>(Resources->SaveIconPixels()[Pixel / 2] >> (4 * (Pixel % 2)) & 15);
		const uint16 Colour = static_cast<uint16>(Resources->SaveIconColours()[2 * Entry] | Resources->SaveIconColours()[2 * Entry + 1] << 8);
		CHECK(Icon->Pixels[Pixel][3] == (Colour == 0 ? 0 : 255));
		CHECK(Icon->Pixels[Pixel][0] == (Colour & 31) * 255 / 31);
		Red += Icon->Pixels[Pixel][0] > 200 && Icon->Pixels[Pixel][1] < 80 ? 1 : 0;
	}
	CHECK(Red > 20);
}
