#include "Media/CdImage.h"
#include "Media/GameLibrary.h"
#include "Support/ChdWriter.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace Weltall;

namespace
{
	std::filesystem::path FreshFolder(const char* Name)
	{
		const std::filesystem::path Folder = std::filesystem::temp_directory_path() / Name;
		std::error_code Error;
		std::filesystem::remove_all(Folder, Error);
		std::filesystem::create_directories(Folder);
		return Folder;
	}

	std::filesystem::path WriteFile(const std::filesystem::path& Path, const std::vector<uint8>& Bytes)
	{
		std::ofstream(Path, std::ios::binary).write(reinterpret_cast<const char*>(Bytes.data()), static_cast<std::streamsize>(Bytes.size()));
		return Path;
	}

	/** Raw Mode 2 sectors of both forms, each byte of their data its own, and a run of zeros a CHD leaves out. */
	std::vector<uint8> RawSectors(uint32 Count)
	{
		std::vector<uint8> Bin(static_cast<std::size_t>(Count) * CdSector::RawSize, 0);
		for (uint32 Lba = 0; Lba < Count; ++Lba)
		{
			if (Lba >= 16 && Lba < 32)
			{
				continue;
			}
			uint8* Sector = Bin.data() + static_cast<std::size_t>(Lba) * CdSector::RawSize;
			std::memset(Sector + 1, 0xFF, 10);
			Sector[15] = 2;
			Sector[16] = 1;
			Sector[18] = Lba % 3 == 2 ? (CdSector::SubmodeVideo | CdSector::SubmodeForm2) : CdSector::SubmodeData;
			std::memcpy(Sector + 20, Sector + 16, 4);
			for (std::size_t Byte = 24; Byte < CdSector::RawSize; ++Byte)
			{
				Sector[Byte] = static_cast<uint8>(Lba * 7 + Byte);
			}
		}
		return Bin;
	}

	/** Every sector of the two images the same, header and data, and a long read across them too. */
	void CheckSameSectors(const FCdImage& Image, const FCdImage& Expected)
	{
		CHECK(Image.IsRaw() == Expected.IsRaw());
		REQUIRE(Image.SectorCount() == Expected.SectorCount());
		for (uint32 Lba = 0; Lba < Expected.SectorCount(); ++Lba)
		{
			INFO("sector " << Lba);
			const auto Header = Image.ReadHeader(Lba);
			const auto ExpectedHeader = Expected.ReadHeader(Lba);
			REQUIRE(Header);
			CHECK(Header->Mode == ExpectedHeader->Mode);
			CHECK(Header->Submode == ExpectedHeader->Submode);
			const auto Data = Image.ReadUserData(Lba);
			REQUIRE(Data);
			CHECK(*Data == *Expected.ReadUserData(Lba));
		}
		const uint64 Long = static_cast<uint64>(Expected.SectorCount() - 2) * CdSector::Form1DataSize + 5;
		const auto Read = Image.ReadForm1(1, Long);
		REQUIRE(Read);
		CHECK(*Read == *Expected.ReadForm1(1, Long));
		CHECK_FALSE(Image.ReadForm1(Expected.SectorCount(), 1));
	}

	std::string OpenError(const std::filesystem::path& Path)
	{
		const TResult<FCdImage> Image = FCdImage::Open(Path);
		return Image ? std::string() : Image.Error().Message;
	}
}

TEST_CASE("A CHD of a CD opens as the BIN it packs", "[Media]")
{
	const std::filesystem::path Folder = FreshFolder("WeltallChdCd");
	// More sectors than the reader takes from the image at a time.
	const std::vector<uint8> Bin = RawSectors(2100);
	const TResult<FCdImage> Plain = FCdImage::Open(WriteFile(Folder / "disc.bin", Bin));
	REQUIRE(Plain);
	const std::vector<uint8> Chd = Testing::MakePlainCdChd(Bin, "MODE2_RAW");
	// Known by what it holds, whatever its name.
	for (const char* Name : {"disc.chd", "disc.img"})
	{
		INFO(Name);
		const TResult<FCdImage> Image = FCdImage::Open(WriteFile(Folder / Name, Chd));
		INFO((Image ? std::string() : Image.Error().Message));
		REQUIRE(Image);
		CheckSameSectors(*Image, *Plain);
	}
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
}

TEST_CASE("A CHD of an image of 2048-byte sectors opens as that image", "[Media]")
{
	const std::filesystem::path Folder = FreshFolder("WeltallChdCooked");
	std::vector<uint8> Iso(9 * CdSector::CookedSize);
	for (std::size_t Index = 0; Index < Iso.size(); ++Index)
	{
		Iso[Index] = static_cast<uint8>(Index * 13 / 7);
	}
	const TResult<FCdImage> Plain = FCdImage::Open(WriteFile(Folder / "disc.iso", Iso));
	const TResult<FCdImage> Image = FCdImage::Open(WriteFile(Folder / "disc.chd", Testing::MakePlainChd(Iso)));
	REQUIRE(Plain);
	REQUIRE(Image);
	CHECK_FALSE(Image->IsRaw());
	CheckSameSectors(*Image, *Plain);
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
}

TEST_CASE("A CHD the game cannot read says why", "[Media]")
{
	const std::filesystem::path Folder = FreshFolder("WeltallChdRefused");
	const std::vector<uint8> Bin = RawSectors(12);
	std::vector<uint8> Old = Testing::MakePlainCdChd(Bin, "MODE2_RAW");
	Testing::WriteBigEndian32(Old, Testing::ChdHeader::Version, 4);
	CHECK(OpenError(WriteFile(Folder / "old.chd", Old)).starts_with("a CHD of an old version"));
	std::vector<uint8> Partial = Testing::MakePlainCdChd(Bin, "MODE2_RAW");
	Testing::WriteBigEndian32(Partial, Testing::ChdHeader::ParentSha1, 1);
	CHECK(OpenError(WriteFile(Folder / "partial.chd", Partial)).starts_with("a CHD that holds only its differences"));
	std::vector<uint8> Copying = Testing::MakePlainCdChd(Bin, "MODE2_RAW");
	Copying.resize(Copying.size() / 2);
	CHECK(OpenError(WriteFile(Folder / "copying.chd", Copying)).starts_with("the CHD ends too soon"));
	CHECK(OpenError(WriteFile(Folder / "sound.chd", Testing::MakePlainCdChd(Bin, "AUDIO"))) == "a CHD of a CD with no data track");
	CHECK(OpenError(WriteFile(Folder / "mixed.chd", Testing::MakePlainCdChd(Bin, "MODE2"))).starts_with("a CHD whose data track keeps 2336 bytes"));
	CHECK(OpenError(WriteFile(Folder / "disk.chd", Testing::MakePlainChd(Bin, 4096, 512))) == "a CHD of something other than a CD");

	// The game library notes each with why; a file named .chd that is no CHD is looked at as any image.
	WriteFile(Folder / "junk.chd", std::vector<uint8>(4096, 'x'));
	const FGameLibrary Library = FGameLibrary::Scan(Folder);
	CHECK(Library.Releases().empty());
	const auto NoteOf = [&Library](const std::filesystem::path& File)
	{
		const auto Found = std::find_if(Library.Notes().begin(), Library.Notes().end(), [&File](const FLibraryNote& Note) { return Note.File == File; });
		return Found == Library.Notes().end() ? std::string() : Found->Text;
	};
	CHECK(NoteOf(Folder / "old.chd") == "Cannot be read: a CHD of an old version (chdman's copy command makes a new one of it).");
	CHECK(NoteOf(Folder / "junk.chd") == "Not a CD's image the game can read.");
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
}
