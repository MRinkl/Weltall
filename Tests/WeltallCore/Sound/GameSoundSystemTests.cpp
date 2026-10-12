#include "Sound/GameSoundSystem.h"
#include "Sound/SoundFiles.h"
#include "Support/SoundRendering.h"
#include "Support/SoundTestFiles.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

using namespace Weltall;

namespace
{
	/**
	 * The game's sound with one effect registered: a note of an instrument whose sample holds the
	 * same value forever (one looping block), attacking at once, so the output turns on when the
	 * note is keyed on.
	 */
	class FSteadyNote
	{
	public:
		FSteadyNote()
			: Sound(std::make_unique<FGameSoundSystem>(Testing::MakeSoundDriverTables()))
		{
			FSoundInstrument Instrument;
			// Attack rate 0 (the fastest, linear), sustain level 15, sustain linear up: the level stays at the top.
			Instrument.Rates = 0x0000F000;
			Instrument.Modes = 0x0311;
			std::vector<uint8> Block(16, 0x77);
			Block[0] = 0x00;
			Block[1] = 0x07;
			auto ParsedBank = FSoundBankFile::Parse(Testing::MakeBankFile(0x30, 0, {Instrument}, Block));
			REQUIRE(ParsedBank);
			Bank = std::move(*ParsedBank);
			auto ParsedEffects = FSoundEffectFile::Parse(Testing::MakeEffectFile(0, 0x30, {{Testing::HexBytes("7F 01 90"), {}}}));
			REQUIRE(ParsedEffects);
			Effects = std::move(*ParsedEffects);
			const FLockedGameSound Locked = Sound->Lock();
			REQUIRE(Locked->LoadBank(Bank, -1) != nullptr);
			REQUIRE(Locked->RegisterEffects(Effects));
		}

		std::vector<int16> Render(int32 Frames, int32 BufferFrames) { return Testing::RenderSound(*Sound, Frames, BufferFrames); }

		/** Renders Before samples, plays the effect, then renders up to Total; returns the first sample heard. */
		int32 FirstHeard(int32 Before, int32 Total)
		{
			std::vector<int16> Samples = Render(Before, 64);
			Sound->Lock()->PlayEffect(0);
			const std::vector<int16> After = Render(Total - Before, 64);
			Samples.insert(Samples.end(), After.begin(), After.end());
			const auto Heard = std::find_if(Samples.begin(), Samples.end(), [](int16 Sample) { return Sample != 0; });
			return Heard == Samples.end() ? -1 : static_cast<int32>((Heard - Samples.begin()) / 2);
		}

		std::unique_ptr<FGameSoundSystem> Sound;
		FSoundBankFile Bank;
		FSoundEffectFile Effects;
	};

	/** Interrupt k (from 1) runs before this sample. */
	int32 SampleOfInterrupt(int32 Interrupt)
	{
		return (Interrupt * FGameSoundClock::CountsPerInterrupt + FGameSoundClock::CountsPerSample - 1) / FGameSoundClock::CountsPerSample;
	}
}

TEST_CASE("A call between renders takes effect at the next interrupt, wherever it falls before it", "[Sound]")
{
	// Interrupt 6 runs before sample 1103.
	REQUIRE(SampleOfInterrupt(6) == 1103);
	const int32 Early = FSteadyNote().FirstHeard(920, 8000);
	const int32 Late = FSteadyNote().FirstHeard(1103, 8000);
	const int32 After = FSteadyNote().FirstHeard(1104, 8000);
	REQUIRE(Early > 0);
	// Anywhere between interrupts 5 and 6 the call is the same to the driver; one sample past
	// interrupt 6, everything happens an interrupt later, at the same sample after it.
	CHECK(Late == Early);
	int32 KeyOn = 1;
	while (SampleOfInterrupt(KeyOn + 1) <= Early)
	{
		++KeyOn;
	}
	CHECK(After - SampleOfInterrupt(KeyOn + 1) == Early - SampleOfInterrupt(KeyOn));
}

TEST_CASE("What a driver call returns is current at once, before any interrupt", "[Sound]")
{
	FSteadyNote Note;
	Note.Render(500, 500);
	const FLockedGameSound Locked = Note.Sound->Lock();
	CHECK(Locked->PlayingEffects(0) == 0);
	Locked->PlayEffect(0);
	CHECK(Locked->PlayingEffects(0) != 0);
}

TEST_CASE("The game's sound comes out the same whatever buffers it is made in", "[Sound]")
{
	FSteadyNote Whole;
	FSteadyNote Pieces;
	Whole.Sound->Lock()->PlayEffect(0);
	Pieces.Sound->Lock()->PlayEffect(0);
	const std::vector<int16> InOne = Whole.Render(20000, 20000);
	const std::vector<int16> InSmall = Pieces.Render(20000, 37);
	CHECK(InOne == InSmall);
	CHECK(std::any_of(InOne.begin(), InOne.end(), [](int16 Sample) { return Sample != 0; }));
}

TEST_CASE("The game thread's calls and the audio thread's samples take turns", "[Sound]")
{
	FSteadyNote Note;
	std::atomic<bool> bDone{false};
	std::thread Audio([&Note, &bDone] {
		std::vector<int16> Buffer(512 * 2);
		for (int32 Pass = 0; Pass < 200; ++Pass)
		{
			Note.Sound->Render(Buffer);
		}
		bDone = true;
	});
	int32 SeenAtOnce = 0;
	int32 Calls = 0;
	while (!bDone)
	{
		{
			const FLockedGameSound Locked = Note.Sound->Lock();
			Locked->PlayEffect(0);
			SeenAtOnce += Locked->PlayingEffects(0) != 0 ? 1 : 0;
			++Calls;
		}
		// A game's frame does other work between its calls.
		std::this_thread::yield();
	}
	Audio.join();
	CHECK(Calls > 0);
	CHECK(SeenAtOnce == Calls);
}
