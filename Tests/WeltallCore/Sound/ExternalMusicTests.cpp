#include "Formats/Wave.h"
#include "Sound/GameSoundSystem.h"
#include "Sound/SoundFiles.h"
#include "Support/SoundRendering.h"
#include "Support/SoundTestFiles.h"

#include <catch_amalgamated.hpp>

#include <algorithm>

using namespace Weltall;

namespace
{
	class FReplacedSong
	{
	public:
		FReplacedSong()
			: Sound(Testing::MakeSoundDriverTables())
		{
			auto Parsed = FSoundSequenceFile::Parse(Testing::MakeSequenceFile({Testing::HexBytes("91 80 FF 90")}));
			REQUIRE(Parsed);
			File = std::move(*Parsed);
			FWave Wave;
			Wave.Samples = {10000, -10000, 20000, -20000, 30000, -30000, 4000, -4000};
			Wave.Loop = FWaveLoop{1, 3};
			File.Replacement = std::make_shared<const FWave>(std::move(Wave));
			Sequence = Sound.Lock()->StartSequence(File, 127, 0);
			REQUIRE(Sequence != nullptr);
		}

		std::vector<int16> Render(int32 Frames, int32 Buffer = 1024) { return Testing::RenderSound(Sound, Frames, Buffer); }
		FGameSoundSystem Sound;
		FSoundSequenceFile File;
		FSoundSequence* Sequence = nullptr;
	};
}

TEST_CASE("External music loops at sampler endpoints independently of render buffers", "[Sound][Assets]")
{
	FReplacedSong Whole;
	FReplacedSong Pieces;
	const auto Samples = Whole.Render(5000, 5000);
	CHECK(Samples == Pieces.Render(5000, 37));
	CHECK(Samples[0] > 0);
	CHECK(Samples[1] == -Samples[0]);
	CHECK(Samples[2] == Samples[6]);
	CHECK(Samples[4] == Samples[8]);
}

TEST_CASE("External music retains stop resume pause volume and output controls", "[Sound][Assets]")
{
	FReplacedSong Song;
	Song.Render(1000);
	Song.Sound.Lock()->Pause();
	const auto Paused = Song.Render(20);
	CHECK(std::all_of(Paused.begin(), Paused.end(), [](int16 Sample) { return Sample == 0; }));
	Song.Sound.Lock()->Resume();
	const auto Resumed = Song.Render(20);
	CHECK(Resumed[0] > 0);
	Song.Sound.Lock()->StopSequence(*Song.Sequence);
	const auto Stopped = Song.Render(20);
	CHECK(std::all_of(Stopped.begin(), Stopped.end(), [](int16 Sample) { return Sample == 0; }));
	Song.Sound.Lock()->ResumeSequence(*Song.Sequence, 127, 0);
	CHECK(Song.Render(20)[0] > 0);
	Song.Sound.Lock()->SetOutputMode(0);
	const auto Mono = Song.Render(20);
	CHECK(std::all_of(Mono.begin(), Mono.end(), [](int16 Sample) { return Sample == 0; }));
	Song.Sound.Lock()->SetOutputMode(2);
	const auto Wide = Song.Render(20);
	CHECK(Wide[0] > 0);
	CHECK(Wide[1] == Wide[0]);
	Song.Sound.Lock()->SetMasterVolume(0, 0);
	const auto Muted = Song.Render(20);
	CHECK(std::all_of(Muted.begin(), Muted.end(), [](int16 Sample) { return Sample == 0; }));
}

TEST_CASE("A track mask uses the native sequence while external music keeps its place", "[Sound][Assets]")
{
	FReplacedSong Song;
	Song.Render(1000);
	Song.Sound.Lock()->MuteTracks(*Song.Sequence, 1);
	const auto Muted = Song.Render(20);
	CHECK(std::all_of(Muted.begin(), Muted.end(), [](int16 Sample) { return Sample == 0; }));
	Song.Sound.Lock()->MuteTracks(*Song.Sequence, 0);
	CHECK(Song.Render(20)[0] > 0);
}

TEST_CASE("External music follows sequence pitch tempo and pan changes", "[Sound][Assets]")
{
	FReplacedSong Normal;
	FReplacedSong Faster;
	FReplacedSong Higher;
	FReplacedSong Panned;
	Faster.Sound.Lock()->SetTempoScale(*Faster.Sequence, 0x200, 0);
	Higher.Sound.Lock()->SetSequencePitch(*Higher.Sequence, 12, 0);
	Panned.Sound.Lock()->SetSequencePan(*Panned.Sequence, 64, 0);
	const auto NormalSamples = Normal.Render(20);
	const auto FastSamples = Faster.Render(20);
	const auto HighSamples = Higher.Render(20);
	CHECK(FastSamples == HighSamples);
	CHECK(FastSamples[2] == NormalSamples[4]);
	const auto PanSamples = Panned.Render(20);
	CHECK(PanSamples[0] == 0);
	CHECK(PanSamples[1] == NormalSamples[1]);
}

TEST_CASE("A one shot WAVE finishes its release after the native ending and an explicit stop cancels it", "[Sound][Assets]")
{
	FGameSoundSystem Sound(Testing::MakeSoundDriverTables());
	auto File = FSoundSequenceFile::Parse(Testing::MakeSequenceFile({Testing::HexBytes("80 01 90")}));
	REQUIRE(File);
	FWave Wave;
	Wave.Samples.assign(8820, 10000);
	File->Replacement = std::make_shared<const FWave>(std::move(Wave));
	FSoundSequence* Sequence = Sound.Lock()->StartSequence(*File, 127, 0);
	REQUIRE(Sequence != nullptr);
	const auto Samples = Testing::RenderSound(Sound, 6000);
	CHECK(Samples[2000] == 10000);
	CHECK(Samples[11000] == 0);
	Sound.Lock()->PlaySequence(*Sequence, 127, 0);
	Testing::RenderSound(Sound, 1000);
	Sound.Lock()->StopSequence(*Sequence);
	const auto Stopped = Testing::RenderSound(Sound, 20);
	CHECK(std::all_of(Stopped.begin(), Stopped.end(), [](int16 Sample) { return Sample == 0; }));
}
