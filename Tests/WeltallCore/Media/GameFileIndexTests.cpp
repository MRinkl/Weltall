#include "Media/GameFileIndex.h"
#include "Support/ByteWriter.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cstring>
#include <string_view>

using namespace Weltall;
using Testing::FByteWriter;

namespace
{
	constexpr uint32 LoadAddress = 0x80010000;
	constexpr std::size_t DirectoryOffset = 0x8004;

	/** An executable whose body holds the index at 80010004 and the directory table at 80018004. */
	FPsxExecutable MakeExecutable(const FByteWriter& Records, std::initializer_list<uint16> Directories)
	{
		std::vector<uint8> Body(DirectoryOffset + FGameFileIndex::DirectoryCount * 2, 0);
		std::memcpy(Body.data() + 4, Records.Bytes.data(), Records.Size());
		std::size_t Offset = DirectoryOffset;
		for (const uint16 Start : Directories)
		{
			Body[Offset] = static_cast<uint8>(Start);
			Body[Offset + 1] = static_cast<uint8>(Start >> 8);
			Offset += 2;
		}
		std::vector<uint8> File(FPsxExecutable::HeaderSize, 0);
		std::memcpy(File.data(), "PS-X EXE", 8);
		const uint32 Fields[] = {LoadAddress, 0, LoadAddress, static_cast<uint32>(Body.size())};
		std::memcpy(File.data() + 0x10, Fields, sizeof(Fields));
		File.insert(File.end(), Body.begin(), Body.end());
		return *FPsxExecutable::Parse(std::move(File));
	}

	FByteWriter& Record(FByteWriter& Writer, uint32 Lba, int32 Size)
	{
		return Writer.U8(static_cast<uint8>(Lba)).U8(static_cast<uint8>(Lba >> 8)).U8(static_cast<uint8>(Lba >> 16)).U32(static_cast<uint32>(Size));
	}
}

TEST_CASE("The file index stops at its end marker and names files from their directory's start", "[Media]")
{
	FByteWriter Records;
	Record(Records, 10, -2);
	Record(Records, 10, 100);
	Record(Records, 11, 200);
	Record(Records, 0xFFFFFF, 0);
	Record(Records, 99, 99);
	// Directory 0 does not exist; directory 1 starts at the 1-based record 1, the group's header.
	const auto Index = FGameFileIndex::Read(MakeExecutable(Records, {0, 1, 0xFFFF}));
	REQUIRE(Index);
	REQUIRE(Index->Records().size() == 3);
	CHECK(Index->Find(0, 1) == nullptr);
	CHECK(Index->Find(2, 1) == nullptr);
	CHECK(*Index->Find(1, 1) == FIndexRecord{10, -2});
	CHECK(*Index->Find(1, 2) == FIndexRecord{10, 100});
	CHECK(*Index->Find(1, 3) == FIndexRecord{11, 200});
	CHECK(Index->Find(1, 4) == nullptr);
	CHECK(Index->Find(1, 0) == nullptr);
	CHECK(Index->Find(FGameFileIndex::DirectoryCount, 1) == nullptr);
}

TEST_CASE("A file's sectors are read in full, padding included", "[Media][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	CHECK(Index->Records().size() == 4151);
	// Directory 0 starts at SYSTEM.CNF, which the index lists like any other file.
	const FIndexRecord* SystemCnf = Index->Find(0, 1);
	REQUIRE(SystemCnf != nullptr);
	CHECK(SystemCnf->Lba == Disc->Find("SYSTEM.CNF")->Lba);
	const auto Sectors = ReadFileSectors(Disc->Image(), *SystemCnf);
	REQUIRE(Sectors);
	CHECK(Sectors->size() == CdSector::Form1DataSize);
	CHECK(std::string(Sectors->begin(), Sectors->begin() + 4) == "BOOT");
	CHECK_FALSE(ReadFileSectors(Disc->Image(), *Index->Find(1, 1)));
}

TEST_CASE("Both discs name the same file the same way", "[Media][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	const auto FirstIndex = FGameFileIndex::Read(*ReadBootExecutable(*First));
	const auto SecondIndex = FGameFileIndex::Read(*ReadBootExecutable(*Second));
	REQUIRE(FirstIndex);
	REQUIRE(SecondIndex);
	CHECK(SecondIndex->Records().size() == 4146);
	const FIndexRecord* OnFirst = FirstIndex->Find(1, 2);
	const FIndexRecord* OnSecond = SecondIndex->Find(1, 2);
	REQUIRE(OnFirst != nullptr);
	REQUIRE(OnSecond != nullptr);
	CHECK(OnFirst->Size == 61680);
	CHECK(OnSecond->Size == 61680);
	CHECK(*ReadFileSectors(First->Image(), *OnFirst) == *ReadFileSectors(Second->Image(), *OnSecond));
}

TEST_CASE("Every file the second disc holds the first disc holds the same, but for its program and movies", "[Media][Content]")
{
	// The field reads the power-on disc after the disc change, the movies the drive's: disc 2 started from disc 1
	// has every file it reads.
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	const auto FirstIndex = FGameFileIndex::Read(*ReadBootExecutable(*First));
	const auto SecondIndex = FGameFileIndex::Read(*ReadBootExecutable(*Second));
	REQUIRE(FirstIndex);
	REQUIRE(SecondIndex);
	// A directory's files run from its first record to the next directory's.
	const auto FileCount = [](const FGameFileIndex& Index, int32 Directory)
	{
		const FIndexRecord* Start = Index.Find(Directory, 1);
		if (Start == nullptr)
		{
			return 0;
		}
		const FIndexRecord* End = Index.Records().data() + Index.Records().size();
		for (int32 Other = 0; Other < FGameFileIndex::DirectoryCount; ++Other)
		{
			const FIndexRecord* OtherStart = Index.Find(Other, 1);
			if (OtherStart != nullptr && OtherStart > Start && OtherStart < End)
			{
				End = OtherStart;
			}
		}
		return static_cast<int32>(End - Start);
	};
	constexpr int32 ProgramDirectory = 0;
	constexpr int32 MovieDirectory = 60;
	constexpr std::string_view Placeholder = "It's CDMAKE Dummy !";
	int32 Same = 0;
	for (int32 Directory = 0; Directory < FGameFileIndex::DirectoryCount; ++Directory)
	{
		if (Directory == ProgramDirectory || Directory == MovieDirectory)
		{
			continue;
		}
		const int32 OnFirstCount = FileCount(*FirstIndex, Directory);
		for (int32 File = 1; File <= FileCount(*SecondIndex, Directory); ++File)
		{
			const FIndexRecord& OnSecond = *SecondIndex->Find(Directory, File);
			if (!OnSecond.IsFile())
			{
				continue;
			}
			const auto SecondBytes = ReadFileSectors(Second->Image(), OnSecond);
			REQUIRE(SecondBytes);
			if (SecondBytes->size() >= Placeholder.size() && std::equal(Placeholder.begin(), Placeholder.end(), SecondBytes->begin()))
			{
				continue;
			}
			INFO("file " << File << " of directory " << Directory);
			REQUIRE(File <= OnFirstCount);
			const FIndexRecord& OnFirst = *FirstIndex->Find(Directory, File);
			REQUIRE(OnFirst.IsFile());
			const auto FirstBytes = ReadFileSectors(First->Image(), OnFirst);
			REQUIRE(FirstBytes);
			CHECK(*FirstBytes == *SecondBytes);
			++Same;
		}
	}
	CHECK(Same == 2648);
}
