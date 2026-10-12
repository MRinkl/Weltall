#include "Media/Iso9660.h"

#include <catch_amalgamated.hpp>

#include <cstring>

using namespace Weltall;

namespace
{
	void PutU32Both(std::vector<uint8>& Bytes, std::size_t Offset, uint32 Value)
	{
		for (int Byte = 0; Byte < 4; ++Byte)
		{
			Bytes[Offset + Byte] = static_cast<uint8>(Value >> (8 * Byte));
			Bytes[Offset + 7 - Byte] = static_cast<uint8>(Value >> (8 * Byte));
		}
	}

	/** A directory record with the XA system-use area PlayStation discs carry. */
	std::vector<uint8> Record(std::string_view Name, uint32 Lba, uint32 Size, bool bDirectory, uint16 XaAttributes)
	{
		const std::size_t NameLength = Name.size();
		const std::size_t Padding = NameLength % 2 == 0 ? 1 : 0;
		std::vector<uint8> Bytes(33 + NameLength + Padding + 14, 0);
		Bytes[0] = static_cast<uint8>(Bytes.size());
		PutU32Both(Bytes, 2, Lba);
		PutU32Both(Bytes, 10, Size);
		Bytes[25] = bDirectory ? 0x02 : 0x00;
		Bytes[32] = static_cast<uint8>(NameLength);
		std::memcpy(Bytes.data() + 33, Name.data(), NameLength);
		const std::size_t SystemUse = 33 + NameLength + Padding;
		Bytes[SystemUse + 4] = static_cast<uint8>(XaAttributes >> 8);
		Bytes[SystemUse + 5] = static_cast<uint8>(XaAttributes);
		Bytes[SystemUse + 6] = 'X';
		Bytes[SystemUse + 7] = 'A';
		return Bytes;
	}

	/** Cooked sectors: descriptor at 16, root directory at 17, subdirectory at 18, file data at 19. */
	FCdImage MakeDisc()
	{
		std::vector<uint8> Bytes(20 * CdSector::CookedSize, 0);
		uint8* Descriptor = Bytes.data() + 16 * CdSector::CookedSize;
		Descriptor[0] = 1;
		std::memcpy(Descriptor + 1, "CD001", 5);
		const auto Root = Record(std::string_view("\0", 1), 17, CdSector::CookedSize, true, XaAttribute::Directory);
		std::memcpy(Descriptor + 156, Root.data(), 34);

		auto Append = [&Bytes](uint32 Lba, std::initializer_list<std::vector<uint8>> Records) {
			std::size_t Offset = Lba * CdSector::CookedSize;
			for (const auto& Entry : Records)
			{
				std::memcpy(Bytes.data() + Offset, Entry.data(), Entry.size());
				Offset += Entry.size();
			}
		};
		Append(17, {Record(std::string_view("\0", 1), 17, CdSector::CookedSize, true, XaAttribute::Directory),
			Record(std::string_view("\1", 1), 17, CdSector::CookedSize, true, XaAttribute::Directory),
			Record("BIN", 18, CdSector::CookedSize, true, XaAttribute::Directory),
			Record("MOVIE.STR;1", 19, 4096, false, XaAttribute::Form2 | XaAttribute::Interleaved)});
		Append(18, {Record(std::string_view("\0", 1), 18, CdSector::CookedSize, true, XaAttribute::Directory),
			Record(std::string_view("\1", 1), 17, CdSector::CookedSize, true, XaAttribute::Directory),
			Record("DATA.BIN;1", 19, 5, false, XaAttribute::Form1)});
		std::memcpy(Bytes.data() + 19 * CdSector::CookedSize, "hello", 5);
		return FCdImage(std::make_shared<FMemoryByteSource>(std::move(Bytes)), CdSector::CookedSize, 0);
	}
}

TEST_CASE("ISO 9660 directories are walked with their XA attributes", "[Media]")
{
	auto Disc = FIso9660::Open(MakeDisc());
	REQUIRE(Disc);
	REQUIRE(Disc->Entries().size() == 3);
	const FIsoEntry* Data = Disc->Find("\\bin\\data.bin;1");
	REQUIRE(Data != nullptr);
	CHECK(Data->Path == "BIN/DATA.BIN");
	CHECK(Data->IsForm1Data());
	const FIsoEntry* Movie = Disc->Find("MOVIE.STR");
	REQUIRE(Movie != nullptr);
	CHECK_FALSE(Movie->IsForm1Data());
	CHECK(Disc->Find("MISSING.BIN") == nullptr);
}

TEST_CASE("ISO 9660 files and ranges read as Form 1 data", "[Media]")
{
	auto Disc = FIso9660::Open(MakeDisc());
	REQUIRE(Disc);
	const auto Data = Disc->ReadFile("BIN/DATA.BIN");
	REQUIRE(Data);
	CHECK(std::string(Data->begin(), Data->end()) == "hello");
	const auto Range = Disc->ReadFileRange(*Disc->Find("BIN/DATA.BIN"), 1, 3);
	REQUIRE(Range);
	CHECK(std::string(Range->begin(), Range->end()) == "ell");
	CHECK_FALSE(Disc->ReadFileRange(*Disc->Find("BIN/DATA.BIN"), 4, 2));
	CHECK_FALSE(Disc->ReadFile("MOVIE.STR"));
}
