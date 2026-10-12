#include "Media/GameFileIndex.h"
#include "Sound/SoundDriver.h"
#include "Sound/SoundDriverTables.h"
#include "Sound/SoundFiles.h"
#include "Support/GameDiscs.h"
#include "Support/RecordingSpu.h"
#include "Support/SoundTestFiles.h"
#include "Support/XorShift32.h"

#include <catch_amalgamated.hpp>

#include <functional>
#include <memory>

using namespace Weltall;

// Digests of the SPU writes the game's own driver makes, run under the oracle harness from the same
// files and calls; the port's driver runs with
// the original's behavior, both fixes off.

namespace
{
	const FSoundDriverOptions Original{false, false};

	/** The game's sound files and the driver booted as the game boots it, writing a recording SPU. */
	class FGameSound
	{
	public:
		explicit FGameSound(const FIso9660& InDisc)
			: Disc(InDisc)
		{
			const auto Executable = ReadBootExecutable(Disc);
			REQUIRE(Executable);
			auto ReadIndex = FGameFileIndex::Read(*Executable);
			REQUIRE(ReadIndex);
			Index = std::make_unique<FGameFileIndex>(std::move(*ReadIndex));
			auto ReadTables = FSoundDriverTables::Read(*Executable);
			REQUIRE(ReadTables);
			Tables = *ReadTables;
			Driver = std::make_unique<FSoundDriver>(Spu, Tables, Original);
			for (const int32 Number : {2, 3, 4, 5})
			{
				BootBanks.push_back(LoadBank(1, Number));
			}
		}

		std::vector<uint8> File(int32 Directory, int32 Number) const
		{
			const FIndexRecord* Record = Index->Find(Directory, Number);
			REQUIRE(Record != nullptr);
			const auto Sectors = ReadFileSectors(Disc.Image(), *Record);
			REQUIRE(Sectors);
			return std::vector<uint8>(Sectors->begin(), Sectors->begin() + Record->Size);
		}

		const FSoundBank* LoadBank(int32 Directory, int32 Number)
		{
			auto Bank = FSoundBankFile::Parse(File(Directory, Number));
			REQUIRE(Bank);
			const FSoundBank* Loaded = Driver->LoadBank(*Bank);
			REQUIRE(Loaded != nullptr);
			return Loaded;
		}

		const FSoundEffectFile& RegisterEffects(int32 Directory, int32 Number)
		{
			auto Parsed = FSoundEffectFile::Parse(File(Directory, Number));
			REQUIRE(Parsed);
			Effects = std::make_unique<FSoundEffectFile>(std::move(*Parsed));
			REQUIRE(Driver->RegisterEffects(*Effects));
			return *Effects;
		}

		FSoundSequence& LoadSequence(std::vector<uint8> Bytes)
		{
			auto Parsed = FSoundSequenceFile::Parse(std::move(Bytes));
			REQUIRE(Parsed);
			Sequence = std::make_unique<FSoundSequenceFile>(std::move(*Parsed));
			FSoundSequence* Loaded = Driver->LoadSequence(*Sequence);
			REQUIRE(Loaded != nullptr);
			return *Loaded;
		}

		void Interrupts(int32 Count)
		{
			for (int32 Interrupt = 0; Interrupt < Count; ++Interrupt)
			{
				Driver->Interrupt();
			}
		}

		const FIso9660& Disc;
		std::unique_ptr<FGameFileIndex> Index;
		FSoundDriverTables Tables;
		Testing::FRecordingSpu Spu;
		std::unique_ptr<FSoundDriver> Driver;
		std::vector<const FSoundBank*> BootBanks;
		std::unique_ptr<FSoundEffectFile> Effects;
		std::unique_ptr<FSoundSequenceFile> Sequence;
	};

	constexpr int32 MusicDirectory = 28;
}

TEST_CASE("The game's sound driver tables are the ones the port computes from their definitions", "[Sound][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Tables = FSoundDriverTables::Read(*Executable);
	REQUIRE(Tables);
	const FSoundDriverTables Defined = Testing::MakeSoundDriverTables();
	CHECK(Tables->Pitches == Defined.Pitches);
	CHECK(Tables->Octaves == Defined.Octaves);
	CHECK(Tables->CommandLengths == Defined.CommandLengths);
	CHECK(Tables->ReverbAreaSizes == Defined.ReverbAreaSizes);
	CHECK(Tables->ReverbPresets[4].Start == 0xF204);
	CHECK(Tables->ReverbPresets[4].Registers[0] == 0x00E3);
}

TEST_CASE("The sound driver starts and loads the boot banks as the game's own does", "[Sound][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FGameSound Game(*Disc);
	CHECK(Game.Spu.Digest() == "b880bcf2889b93ab8e7589f789a0676e9bc26d02");
	CHECK(Game.Spu.WriteCount() == 227);
	REQUIRE(Game.BootBanks.size() == 4);
	CHECK(FSoundDriver::BankAddress(*Game.BootBanks[0]) == 0x1010);
	CHECK(FSoundDriver::BankAddress(*Game.BootBanks[1]) == 0x12000);
	CHECK(FSoundDriver::BankAddress(*Game.BootBanks[2]) == 0x72000);
	CHECK(FSoundDriver::BankAddress(*Game.BootBanks[3]) == 0x68000);
	// The samples went into sound RAM through DMA.
	const auto Bank = FSoundBankFile::Parse(Game.File(1, 3));
	REQUIRE(Bank);
	const auto Ram = Game.Spu.Ram().subspan(0x12000, Bank->Samples().size());
	CHECK(std::equal(Ram.begin(), Ram.end(), Bank->Samples().begin()));
}

TEST_CASE("The sound driver plays songs as the game's own does, quirks included", "[Sound][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	struct FSong
	{
		int32 Number;
		const char* Digest;
		int32 Writes;
	};
	// 4000 interrupts each; song 17 plays notes above the SPU's range.
	const FSong Songs[] = {
		{0, "db9c3c53b2442193e9a3a28007547b16e67e916c", 3298}, {6, "4bb36560fd799cff7f5373c13fdabebc3ffb20b8", 17929},
		{9, "cda01572d28f33d4272e376a5e87315038258443", 49793}, {17, "84a628279ffb9d82e2fce24be327f5eee8d0017d", 145874},
		{20, "f5c76ed2ee94071cfd638db9206eaf973ef01e2a", 4683}, {23, "e63364ea2014c735ff4093e6c4f23daac98363e5", 62591},
		{36, "20560b2adeb07815a527a758ec3cbb67dcc708c3", 36151}, {45, "b0816ceaa3af80fbe0351c66195bc2462ae6b0cb", 4750},
		{59, "274a852c378580e8b347cc1a994bbc44af00fd5b", 3127}, {70, "b6f578d190a9d5aee40a7558a46310fc2af275d2", 3792},
	};
	for (const FSong& Song : Songs)
	{
		INFO("song " << Song.Number);
		FGameSound Game(*Disc);
		Game.Spu.Restart();
		Game.LoadBank(MusicDirectory, 0x13 + 2 * Song.Number);
		FSoundSequence& Sequence = Game.LoadSequence(Game.File(MusicDirectory, 0x14 + 2 * Song.Number));
		Game.Driver->PlaySequence(Sequence, 0x7F, 0);
		Game.Interrupts(4000);
		CHECK(Game.Spu.Digest() == Song.Digest);
		CHECK(Game.Spu.WriteCount() == Song.Writes);
	}
}

TEST_CASE("The sound driver plays effects as the game's own does", "[Sound][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FGameSound Game(*Disc);
	Game.Spu.Restart();
	const FSoundEffectFile& Effects = Game.RegisterEffects(4, 168);
	Testing::FXorShift32 Random(0x50D1E5);
	for (int32 Step = 0; Step < 120; ++Step)
	{
		const uint32 Effect = Random.Next() % static_cast<uint32>(Effects.EffectCount());
		if (Step % 3 == 2)
		{
			Game.Driver->PlayEffectOn(Effect, static_cast<int32>(Random.Next() % 8 * 2));
		}
		else
		{
			Game.Driver->PlayEffect(Effect);
		}
		Game.Interrupts(25);
	}
	CHECK(Game.Spu.Digest() == "0c21e52a9e3ac49b5a19db6a86cb1ace130cd180");
	CHECK(Game.Spu.WriteCount() == 29076);
}

TEST_CASE("The sound driver runs every other command and entry point as the game's own does", "[Sound][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FGameSound Game(*Disc);
	Game.Spu.Restart();
	Game.RegisterEffects(4, 168);
	// What the game's data never exercise: unused commands, percussion, voice moves, noise, LFO
	// slots, a jump into an effect's commands, the tempo freeze, the end-when-silent test.
	FSoundSequence& Sequence = Game.LoadSequence(Testing::MakeSequenceFile({
		Testing::HexBytes("a060 970304 a405 a502 8a 8f 8e010203 bc010203 bd be a70450 8030 a67f 8030 f90201 fd02 8060 fd01 a21070 8060 a110 8030 91 80c0 90"),
		Testing::HexBytes("fe00 ac01 c1050703 c308 c60a c801 c905 ca07 c0 9405 5a0c aa05 5a1f b6 5a32 b7 d41002 5a0c dc 5a0c ec102000 5a0c ef 5a0c ee 5a0c "
						  "f0030300 f1081000 f20205 f603 5a0c f703 d5 d408fe 5a0c d5 5a0c 8d00 5a0c e1f0 5a0c ae 5a3b af f5 8030 9c010000 8030 9d0100 8030 90"),
		Testing::HexBytes("e820 e910 ea1060 a908 ad05 9404 9803 5a06 9a 5a07 99 ad00 a910 5a06 9e020000"),
		Testing::HexBytes("9405 5a06 ff 90"),
	}, {63, 2, 60, 0, 0x40}));
	FSoundDriver& Driver = *Game.Driver;
	std::vector<uint32> Playing;
	const std::vector<std::pair<int32, std::function<void()>>> Timeline = {
		{0, [&] { Driver.PlaySequence(Sequence, 0x7F, 0); }},
		{50, [&] { Driver.SetSequencePitch(Sequence, 2, 0); }},
		{80, [&] { Driver.SetSequencePitch(Sequence, -1, 100); }},
		{120, [&] { Driver.SetSequencePan(Sequence, 0x20, 0); }},
		{150, [&] { Driver.SetSequencePan(Sequence, -0x10, 50); }},
		{200, [&] { Driver.MuteTracks(Sequence, 0b0110); }},
		{260, [&] { Driver.MuteTracks(Sequence, 0); }},
		{300, [&] { Driver.SetOutputMode(0); }},
		{340, [&] { Driver.SetOutputMode(2); }},
		{380, [&] { Driver.SetOutputMode(3); }},
		{420, [&] { Driver.SetOutputMode(1); }},
		{450, [&] { Driver.SetMasterVolume(0x2000, 20); }},
		{500, [&] { Driver.SetMasterVolume(0x3FFF, 0); }},
		{520, [&] { Driver.SetCdVolume(0x4000, 30); }},
		{560, [&] { Driver.Pause(); }},
		{580, [&] { Driver.Resume(); }},
		{600, [&] { Driver.SetSequenceVolume(Sequence, 0, 30); }},
		{700, [&] { Driver.SetSequenceVolume(Sequence, 0x7F, 20); }},
		{760, [&] { Driver.StopSequence(Sequence); }},
		{780, [&] { Driver.ResumeSequence(Sequence, 0x7F, 40); }},
		{850, [&] { Driver.SetTempoScale(Sequence, 0x200, 50); }},
		{950, [&] { Driver.SetTempoScale(Sequence, 0, 0); }},
		{1000, [&] { Driver.PlaySystemEffect(3); }},
		{1010, [&] { Driver.PlayProtectedEffect(5); }},
		{1020, [&] { Driver.PlayEffectVolumePan(7, 0x60, 0x20); }},
		{1030, [&] { Driver.PlayEffectOnVolumePan(9, 6, 0x50, 0x70); }},
		{1040, [&] { Driver.SetEffectVolume(7, 0x30); }},
		{1045, [&] { Driver.SetEffectVolumeOn(6, 0x20); }},
		{1050, [&] { Driver.FadeEffectVolume(3, 0x10, 30); }},
		{1055, [&] { Driver.FadeEffectVolumeOn(6, 0x7F, 10); }},
		{1060, [&] { Driver.SetEffectPanOn(6, 0x10); }},
		{1070, [&] { Playing.push_back(Driver.PlayingEffects(FSoundDriver::AnyEffect)); }},
		{1072, [&] { Playing.push_back(Driver.PlayingEffects(3)); }},
		{1080, [&] { Driver.StopEffect(7); }},
		{1085, [&] { Driver.StopEffectOn(6); }},
		{1090, [&] { Driver.StopAllEffects(); }},
		{1100, [&] { Driver.StopAllSequences(); }},
	};
	std::size_t Next = 0;
	for (int32 Interrupt = 0; Interrupt < 2400; ++Interrupt)
	{
		if (Next < Timeline.size() && Timeline[Next].first == Interrupt)
		{
			Timeline[Next++].second();
		}
		Driver.Interrupt();
	}
	CHECK(Game.Spu.Digest() == "692a1c80a6e17b2d4dfdefcb502e11a293f8865f");
	CHECK(Game.Spu.WriteCount() == 3403);
	CHECK(Playing == std::vector<uint32>{0xC000, 0});
}
