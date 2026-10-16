#include "Foundation/AssetKey.h"
#include "Media/AssetFiles.h"
#include "Media/DiscAssets.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Sound/GameSoundSession.h"
#include "Sound/MusicReplacements.h"
#include "Sound/MusicWaveExport.h"
#include "Support/GameDiscs.h"
#include "Support/SoundRendering.h"

#include <catch_amalgamated.hpp>

#include <filesystem>
#include <optional>
#include <set>
#include <string_view>

using namespace Weltall;

TEST_CASE("Every field song can start a music WAVE export", "[MusicExport][Content][Assets]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = FGameSoundSession::Start(*Disc);
	REQUIRE(Session);
	REQUIRE((*Session)->FieldMusic().SongCount() == 73);
	for (int32 Song = 0; Song < (*Session)->FieldMusic().SongCount(); ++Song)
	{
		INFO("song " << Song);
		const auto Export = FMusicWaveExport::Start(**Session, Song);
		REQUIRE(Export);
	}
}

TEST_CASE("A recording named by its sequence's key replaces that song wherever the discs keep it", "[MusicExport][Content][Assets]")
{
	REQUIRE_DISC(Original, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Original);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const FIndexRecord* Record = Index->Find(28, 0x14 + 2 * FFieldMusic::DefeatSong);
	REQUIRE(Record != nullptr);
	auto Sequence = ReadFileSectors(Original->Image(), *Record);
	REQUIRE(Sequence);
	Sequence->resize(static_cast<std::size_t>(Record->Size));
	const auto Folder = std::filesystem::temp_directory_path() / "WeltallMusicWaveReplacement";
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
	{
		FIso9660 Disc = *Original;
		const auto Store = FDiscAssetStore::Open(Folder, "usa.disc1", Disc.Image(), *Index);
		REQUIRE(Store);
		FWave Wave;
		Wave.Samples.assign(100, 10000);
		const auto Bytes = Wave.Serialize();
		REQUIRE(Bytes);
		REQUIRE(PublishAssetFile(Folder, std::filesystem::path("music") / ("defeat-" + AssetKeyName(MusicSequenceKey(*Sequence)) + ".wav"), *Bytes));
		const auto Music = FMusicReplacements::Find(Folder / "music");
		REQUIRE(Music);
		(*Store)->SetMusic(*Music);
		(*Store)->SetEnabled(true);
		Disc.Image().SetAssetStore(*Store);
		auto Session = FGameSoundSession::Start(Disc);
		REQUIRE(Session);
		REQUIRE((*Session)->FieldMusic().ChangeSong(FFieldMusic::DefeatSong, EFieldSongStart::FullVolume));
		const auto Samples = Testing::RenderSound(*(*Session)->Sound(), 2048);
		CHECK(Samples[2000] == 10000);
		CHECK(Samples[2001] == 10000);
	}
	std::filesystem::remove_all(Folder, Error);
}

TEST_CASE("Every world map song is a copy of a field song, so one recording serves both", "[MusicExport][Content][Assets]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto KeyOf = [&](int32 Directory, int32 Number) -> std::optional<uint64>
	{
		const FIndexRecord* Record = Index->Find(Directory, Number);
		if (Record == nullptr || !Record->IsFile() || Record->Size < 4) return std::nullopt;
		auto Bytes = ReadFileSectors(Disc->Image(), *Record);
		REQUIRE(Bytes);
		Bytes->resize(static_cast<std::size_t>(Record->Size));
		if (std::string_view(reinterpret_cast<const char*>(Bytes->data()), 4) != "smds") return std::nullopt;
		return MusicSequenceKey(*Bytes);
	};
	std::set<uint64> Field;
	for (int32 Song = 0; Song < 80; ++Song)
	{
		if (const auto Key = KeyOf(28, 0x14 + 2 * Song)) Field.insert(*Key);
	}
	int32 WorldMap = 0;
	for (int32 Number = 0; Number < 400; ++Number)
	{
		if (const auto Key = KeyOf(36, Number))
		{
			++WorldMap;
			CHECK(Field.contains(*Key));
		}
	}
	CHECK(WorldMap == 30);
}

TEST_CASE("Music export finishes the native ending or full loop in bounded steps", "[MusicExport][Content][Assets]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = FGameSoundSession::Start(*Disc);
	REQUIRE(Session);
	auto Export = FMusicWaveExport::Start(**Session, FFieldMusic::DefeatSong);
	REQUIRE(Export);
	bool bDone = false;
	for (int32 Second = 0; Second < 120 && !bDone; ++Second)
	{
		const std::size_t Before = (*Export)->Wave().Frames();
		const auto Step = (*Export)->Step(44100);
		REQUIRE(Step);
		bDone = *Step;
		CHECK((*Export)->Wave().Frames() <= Before + 44100);
	}
	REQUIRE(bDone);
	REQUIRE((*Export)->Wave().Frames() > 44100);
	const auto Bytes = (*Export)->Wave().Serialize();
	REQUIRE(Bytes);
	const auto Decoded = FWave::Parse(*Bytes);
	REQUIRE(Decoded);
	CHECK(Decoded->Frames() == (*Export)->Wave().Frames());
	if (Decoded->Loop) CHECK(Decoded->Loop->EndFrame == Decoded->Frames());
}
