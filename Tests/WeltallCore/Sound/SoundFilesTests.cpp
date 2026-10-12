#include "Foundation/LittleEndian.h"
#include "Media/GameFileIndex.h"
#include "Sound/SoundFiles.h"
#include "Support/GameDiscs.h"
#include "Support/SoundTestFiles.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;
using Testing::HexBytes;
using Testing::MakeBankFile;
using Testing::MakeEffectFile;
using Testing::MakeSequenceFile;

TEST_CASE("Sound banks give their instruments, their sample data and where they want to go", "[Sound]")
{
	const std::vector<uint8> Samples{1, 2, 3, 4, 5, 6, 7, 8};
	const auto Bank = FSoundBankFile::Parse(MakeBankFile(0x26, 0x12000, {{2, 5, -3, 0x1F7F3F7F, 0x0357}, {9, 0, 12, 0, 0}}, Samples));
	REQUIRE(Bank);
	CHECK(Bank->Id() == 0x26);
	CHECK(Bank->RequestedAddress() == 0x12000);
	REQUIRE(Bank->Instruments().size() == 2);
	CHECK(Bank->Instruments()[0].SampleStart == 2);
	CHECK(Bank->Instruments()[0].LoopStart == 5);
	CHECK(Bank->Instruments()[0].Tuning == -3);
	CHECK(Bank->Instruments()[0].Rates == 0x1F7F3F7Fu);
	CHECK(Bank->Instruments()[0].Modes == 0x0357);
	CHECK(Bank->Instruments()[1].Tuning == 12);
	CHECK(std::vector<uint8>(Bank->Samples().begin(), Bank->Samples().end()) == Samples);
}

TEST_CASE("Sound banks whose header or samples run past the file are refused", "[Sound]")
{
	std::vector<uint8> Bank = MakeBankFile(1, 0, {{}}, {1, 2, 3, 4});
	std::vector<uint8> Samples = Bank;
	LittleEndian::WriteU32(Samples, 0x14, 5);
	CHECK_FALSE(FSoundBankFile::Parse(Samples));
	std::vector<uint8> Header = Bank;
	LittleEndian::WriteU32(Header, 0x10, static_cast<uint32>(Bank.size() + 1));
	CHECK_FALSE(FSoundBankFile::Parse(Header));
	std::vector<uint8> Magic = Bank;
	Magic[0] = 'x';
	CHECK_FALSE(FSoundBankFile::Parse(Magic));
}

TEST_CASE("Sequences give their tracks, their percussion keys and the reverb they ask for", "[Sound]")
{
	// Track 0 ends at once; track 1 is empty; track 2 plays a note with an explicit length, then ends.
	const auto Sequence = FSoundSequenceFile::Parse(MakeSequenceFile({HexBytes("90"), {}, HexBytes("7f 00 30 90")}, {63, 2, 60, 0, 0x40}));
	REQUIRE(Sequence);
	CHECK(Sequence->TrackCount() == 3);
	CHECK(Sequence->TrackOffset(1) == 0);
	CHECK(Sequence->Bytes()[Sequence->TrackOffset(2)] == 0x7F);
	CHECK(Sequence->HasPercussion());
	CHECK(Sequence->Percussion(63).Instrument == 2);
	CHECK(Sequence->Percussion(63).Key == 60);
	CHECK(Sequence->Percussion(63).Pan == 0x40);
	CHECK(Sequence->Percussion(62).Instrument == 0);
	CHECK(Sequence->ReverbType() == 4);
	CHECK(Sequence->ReverbDepth() == 0x20);
}

TEST_CASE("Sequences the driver could not read safely are refused", "[Sound]")
{
	// Streams that run past the file, hold a byte that is no command, divide by a zero beat, or
	// carry a note byte past the note tables.
	CHECK_FALSE(FSoundSequenceFile::Parse(MakeSequenceFile({HexBytes("7f 0c")})));
	CHECK_FALSE(FSoundSequenceFile::Parse(MakeSequenceFile({HexBytes("82 90")})));
	CHECK_FALSE(FSoundSequenceFile::Parse(MakeSequenceFile({HexBytes("97 04 00 90")})));
	CHECK_FALSE(FSoundSequenceFile::Parse(MakeSequenceFile({HexBytes("7f e4 90")})));
	CHECK_FALSE(FSoundSequenceFile::Parse(MakeSequenceFile({HexBytes("7f 00")})));
	// A percussion key past the table, no tracks, too many, and an unknown reverb type.
	CHECK_FALSE(FSoundSequenceFile::Parse(MakeSequenceFile({HexBytes("90")}, {96, 0, 0, 0, 0})));
	CHECK_FALSE(FSoundSequenceFile::Parse(MakeSequenceFile({})));
	CHECK_FALSE(FSoundSequenceFile::Parse(MakeSequenceFile(std::vector<std::vector<uint8>>(33, HexBytes("90")))));
	std::vector<uint8> Reverb = MakeSequenceFile({HexBytes("90")});
	Reverb[0x1A] = 10;
	CHECK_FALSE(FSoundSequenceFile::Parse(Reverb));
	// A jump into an effect ends a stream as well as an end does.
	CHECK(FSoundSequenceFile::Parse(MakeSequenceFile({HexBytes("9e 01 00 00")})));
}

TEST_CASE("Effect files give each effect's channels and volume, and where commands can go on", "[Sound]")
{
	const auto Effects = FSoundEffectFile::Parse(MakeEffectFile(6, 0, {{HexBytes("fc 00 01 7f 0c 90"), {}}, {HexBytes("ba 90"), HexBytes("80 10 90")}}));
	REQUIRE(Effects);
	CHECK(Effects->FileId() == 6);
	CHECK(Effects->EffectCount() == 2);
	CHECK(Effects->Volume(1) == 0x7F);
	CHECK(Effects->ChannelOffset(1) == 0);
	CHECK(Effects->Bytes()[Effects->ChannelOffset(2)] == 0xBA);
	// Three bytes into the first channel is its note; three into BA 90 is past its end.
	CHECK(Effects->CanContinue(0));
	CHECK_FALSE(Effects->CanContinue(1));
	CHECK_FALSE(Effects->CanContinue(2));
}

TEST_CASE("Effect files with a wrong checksum or version are refused, as the driver refuses them", "[Sound]")
{
	const std::vector<uint8> Good = MakeEffectFile(1, 0, {{HexBytes("90"), {}}});
	REQUIRE(FSoundEffectFile::Parse(Good));
	std::vector<uint8> Sum = Good;
	++Sum[4];
	CHECK_FALSE(FSoundEffectFile::Parse(Sum));
	std::vector<uint8> Version = Good;
	Version[0x0C] = 0;
	Version[4] = static_cast<uint8>(Version[4] + 1);
	CHECK_FALSE(FSoundEffectFile::Parse(Version));
	CHECK_FALSE(FSoundEffectFile::Parse(MakeEffectFile(1, 0, {{HexBytes("80"), {}}})));
}

TEST_CASE("Every sound file of the game reads as its driver reads it", "[Sound][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	int32 Banks = 0;
	int32 Sequences = 0;
	int32 EffectFiles = 0;
	for (const FIndexRecord& Record : Index->Records())
	{
		if (!Record.IsFile())
		{
			continue;
		}
		const auto Sectors = ReadFileSectors(Disc->Image(), Record);
		REQUIRE(Sectors);
		std::vector<uint8> File(Sectors->begin(), Sectors->begin() + Record.Size);
		const std::string Magic(File.begin(), File.begin() + std::min<std::size_t>(4, File.size()));
		INFO("file at sector " << Record.Lba);
		if (Magic == "wds ")
		{
			CHECK(FSoundBankFile::Parse(std::move(File)));
			++Banks;
		}
		else if (Magic == "smds")
		{
			CHECK(FSoundSequenceFile::Parse(std::move(File)));
			++Sequences;
		}
		else if (Magic == "seds")
		{
			CHECK(FSoundEffectFile::Parse(std::move(File)));
			++EffectFiles;
		}
	}
	CHECK(Banks == 153);
	CHECK(Sequences == 106);
	CHECK(EffectFiles == 46);
}
