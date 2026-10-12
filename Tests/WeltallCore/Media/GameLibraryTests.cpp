#include "Media/GameLibrary.h"
#include "Support/ChdWriter.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace Weltall;

TEST_CASE("The game library skips files that are not readable discs", "[Media]")
{
	const std::filesystem::path Folder = std::filesystem::temp_directory_path() / "WeltallGameLibraryTest";
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
	std::filesystem::create_directories(Folder / "Nested", Error);
	REQUIRE_FALSE(Error);
	std::ofstream(Folder / "Nested" / "junk.bin", std::ios::binary) << std::string(4096, 'x');
	std::ofstream(Folder / "missing.cue") << "FILE \"nowhere.bin\" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n";
	std::ofstream(Folder / "notes.txt") << "not a disc";
	std::ofstream(Folder / "eboot.PBP", std::ios::binary) << std::string(64, 'x');
	std::ofstream(Folder / "game.chd", std::ios::binary) << std::string(64, 'x');

	const FGameLibrary Library = FGameLibrary::Scan(Folder);
	CHECK(Library.Releases().empty());
	CHECK(Library.Find(EDiscRelease::UsaDisc1) == nullptr);
	// Each file it cannot use is noted, with why; a file that is no disc image of any kind is passed over.
	const auto NoteOf = [&Library](const std::filesystem::path& File)
	{
		const auto Found = std::find_if(Library.Notes().begin(), Library.Notes().end(), [&File](const FLibraryNote& Note) { return Note.File == File; });
		return Found == Library.Notes().end() ? std::string() : Found->Text;
	};
	CHECK(Library.Notes().size() == 4);
	CHECK(NoteOf(Folder / "Nested" / "junk.bin") == "Not a CD's image the game can read.");
	CHECK(NoteOf(Folder / "missing.cue").starts_with("Cannot be read: "));
	CHECK(NoteOf(Folder / "eboot.PBP").starts_with("A PSP eboot"));
	CHECK(NoteOf(Folder / "game.chd") == "Not a CD's image the game can read.");
	CHECK(NoteOf(Folder / "notes.txt").empty());
	CHECK(FGameLibrary::Scan(Folder / "absent").Releases().empty());
	std::filesystem::remove_all(Folder, Error);
}

TEST_CASE("The game library names the discs it does not play and keeps the ones the game reads as it looks again", "[Media][Content]")
{
	REQUIRE_DISC(Disc1, EDiscRelease::UsaDisc1);
	FGameLibrary Library = FGameLibrary::Scan(WELTALL_GAMES_DIR);
	const FIso9660* Found = Library.Find(EDiscRelease::UsaDisc1);
	REQUIRE(Found != nullptr);
	CHECK_FALSE(Library.FileOf(EDiscRelease::UsaDisc1).empty());
	// Whatever else lies beside the USA discs (other releases, packed copies) is noted with why; the discs played are not.
	for (const FLibraryNote& Note : Library.Notes())
	{
		INFO(Note.File.string() << ": " << Note.Text);
		CHECK_FALSE(Note.Text.empty());
		CHECK(Note.File != Library.FileOf(EDiscRelease::UsaDisc1));
		CHECK(Note.File != Library.FileOf(EDiscRelease::UsaDisc2));
	}
	Library.Adopt(FGameLibrary::Scan(WELTALL_GAMES_DIR));
	CHECK(Library.Find(EDiscRelease::UsaDisc1) == Found);
}

TEST_CASE("The game library passes over a disc whose image ends before its last file, as one still being copied does", "[Media][Content]")
{
	REQUIRE_DISC(Disc1, EDiscRelease::UsaDisc1);
	// Disc 1 copied as far as its directories and program, which tell which disc it is, but not the files after them.
	uint32 Copied = 0;
	for (const FIsoEntry& Entry : Disc1->Entries())
	{
		if (Entry.bDirectory || Entry.Path == "SYSTEM.CNF" || Entry.Path == "SLUS_006.64")
		{
			Copied = std::max(Copied, Entry.Lba + Entry.SectorCount());
		}
	}
	REQUIRE(Copied + 1000 < Disc1->Image().SectorCount());
	const std::filesystem::path Folder = std::filesystem::temp_directory_path() / "WeltallGameLibraryCopying";
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
	std::filesystem::create_directories(Folder, Error);
	const auto CheckPassedOver = [&Folder](const std::filesystem::path& File)
	{
		const FGameLibrary Library = FGameLibrary::Scan(Folder);
		CHECK(Library.Find(EDiscRelease::UsaDisc1) == nullptr);
		REQUIRE(Library.Notes().size() == 1);
		CHECK(Library.Notes()[0].File == File);
		CHECK(Library.Notes()[0].Text == "Ends too soon: still being copied, or a rip cut short.");
		CHECK_FALSE(Library.Notes()[0].bPlays);
		std::error_code Removed;
		std::filesystem::remove(File, Removed);
	};
	SECTION("Its data alone, ending where the copy stopped")
	{
		const std::filesystem::path Iso = Folder / "Xenogears (Disc 1).iso";
		{
			std::ofstream File(Iso, std::ios::binary);
			for (uint32 Lba = 0; Lba < Copied; ++Lba)
			{
				auto Data = Disc1->Image().ReadUserData(Lba);
				REQUIRE(Data);
				Data->resize(CdSector::Form1DataSize);
				File.write(reinterpret_cast<const char*>(Data->data()), static_cast<std::streamsize>(Data->size()));
			}
		}
		CheckPassedOver(Iso);
	}
	const auto RawCopied = [&Disc1, Copied]()
	{
		std::vector<uint8> Bin;
		for (uint32 Lba = 0; Lba < Copied; ++Lba)
		{
			const auto Header = Disc1->Image().ReadHeader(Lba);
			auto Data = Disc1->Image().ReadUserData(Lba);
			REQUIRE(Header);
			REQUIRE(Data);
			std::vector<uint8> Sector(CdSector::RawSize, 0);
			std::fill(Sector.begin() + 1, Sector.begin() + 11, uint8{0xFF});
			Sector[15] = Header->Mode;
			const std::array<uint8, 4> Subheader{Header->FileNumber, Header->Channel, Header->Submode, Header->CodingInfo};
			std::copy(Subheader.begin(), Subheader.end(), Sector.begin() + 16);
			std::copy(Subheader.begin(), Subheader.end(), Sector.begin() + 20);
			std::copy(Data->begin(), Data->end(), Sector.begin() + 24);
			Bin.insert(Bin.end(), Sector.begin(), Sector.end());
		}
		return Bin;
	};
	const auto Write = [](const std::filesystem::path& Path, const std::vector<uint8>& Bytes)
	{
		std::ofstream(Path, std::ios::binary).write(reinterpret_cast<const char*>(Bytes.data()), static_cast<std::streamsize>(Bytes.size()));
	};
	SECTION("Raw sectors in a file already the whole disc's size, zeros where the copy has not reached")
	{
		const std::filesystem::path Bin = Folder / "Xenogears (Disc 1).bin";
		Write(Bin, RawCopied());
		std::filesystem::resize_file(Bin, static_cast<uint64>(Disc1->Image().SectorCount()) * CdSector::RawSize);
		CheckPassedOver(Bin);
	}
	SECTION("A CHD of the sectors copied so far: the disc is known through it, and passed over")
	{
		const std::filesystem::path Chd = Folder / "Xenogears (Disc 1).chd";
		Write(Chd, Testing::MakePlainCdChd(RawCopied(), "MODE2_RAW"));
		CheckPassedOver(Chd);
	}
	std::filesystem::remove_all(Folder, Error);
}
