#include "Media/GameFileIndex.h"
#include "Media/MoviePlayer.h"
#include "Media/PsxExecutable.h"
#include "Sound/GameSoundSystem.h"
#include "Sound/SoundDriverTables.h"
#include "Support/GameDiscs.h"
#include "Timing/PsxTiming.h"

#include <catch_amalgamated.hpp>

#include <memory>
#include <vector>

using namespace Weltall;

namespace
{
	constexpr uint32 MovieSectorBytes = 2336;
	constexpr FPadState NoButtons{};
	constexpr FPadState Cross{FPadState::Bit(EPadButton::Cross)};

	/** The game's sound, started as the game starts it. */
	std::unique_ptr<FGameSoundSystem> MakeSound(const FIso9660& Disc)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		const auto Tables = FSoundDriverTables::Read(*Executable);
		REQUIRE(Tables);
		return std::make_unique<FGameSoundSystem>(*Tables);
	}

	/** Disc 1's record 16 (movie 15): 137 pictures. */
	FMovieLocation MovieFifteen(const FIso9660& Disc)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		const FIndexRecord Movie = Index->Records()[16];
		return {Movie.Lba, static_cast<uint32>(Movie.Size) / MovieSectorBytes};
	}

	FMoviePlayer MakePlayer(const FIso9660& Disc, FGameSoundSystem& Sound, bool bSkippable, uint32 EndFrame = FMoviePlayback::DefaultEndFrame)
	{
		FMoviePlayback Playback;
		Playback.EndFrame = EndFrame;
		Playback.bSkippable = bSkippable;
		return FMoviePlayer(Disc.Image(), MovieFifteen(Disc), Playback, Sound);
	}

	/** Lets the game's sound play at least the interrupts (at most one more); returns the CD input's volume on the left. */
	int16 PlayInterrupts(FGameSoundSystem& Sound, int32 Interrupts)
	{
		std::vector<int16> Stereo(static_cast<std::size_t>(Interrupts) * 184 * 2);
		Sound.Render(Stereo);
		constexpr uint32 CdVolumeLeft = 0x1B0;
		return static_cast<int16>(Sound.Lock().Spu().Read(CdVolumeLeft));
	}

	/** Advances one vertical blank at a time until the movie stops; returns the blanks it took. */
	int32 PlayOut(FMoviePlayer& Player, FPadState Held, int32 Limit)
	{
		for (int32 Vblank = 1; Vblank <= Limit; ++Vblank)
		{
			const auto Playing = Player.Vblank(Held);
			REQUIRE(Playing);
			if (!*Playing)
			{
				return Vblank;
			}
		}
		return -1;
	}

	/** Plays vertical blanks until the movie shows picture Number. */
	void ShowPicture(FMoviePlayer& Player, uint32 Number)
	{
		while (Player.PictureNumber() < Number)
		{
			const auto Playing = Player.Vblank(NoButtons);
			REQUIRE(Playing);
			REQUIRE(*Playing);
		}
	}
}

TEST_CASE("A movie shows its pictures at 15 a second and stops after the last", "[Media][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Sound = MakeSound(*Disc);
	FMoviePlayer Player = MakePlayer(*Disc, *Sound, true);
	const auto Started = Player.Start();
	REQUIRE(Started);
	CHECK(*Started);
	REQUIRE(Player.Picture() != nullptr);
	CHECK(Player.Picture()->Width == 320);
	// 137 pictures of 1/15 s, in vertical blanks of the NTSC display.
	const int32 Expected = static_cast<int32>(137.0 / 15.0 / PsxTiming::FrameSeconds) + 1;
	const int32 Blanks = PlayOut(Player, NoButtons, 1000);
	CHECK(Blanks >= Expected - 1);
	CHECK(Blanks <= Expected + 1);
	CHECK(Player.IsFinished());
}

TEST_CASE("Cross skips a movie after 5 vertical blanks; held from the start, once it repeats", "[Media][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Sound = MakeSound(*Disc);
	FMoviePlayer Pressed = MakePlayer(*Disc, *Sound, true);
	REQUIRE(Pressed.Start());
	REQUIRE(PlayOut(Pressed, NoButtons, 60) == -1);
	CHECK(PlayOut(Pressed, Cross, 100) == 5);

	// The first reading sees no change from the program's all-buttons start; 91 unchanged readings
	// later the auto-repeat counts Cross as pressed, at the 92nd blank.
	FMoviePlayer HeldFromStart = MakePlayer(*Disc, *Sound, true);
	REQUIRE(HeldFromStart.Start());
	CHECK(PlayOut(HeldFromStart, Cross, 200) == 92 + 4);
}

TEST_CASE("A movie that forbids skipping plays on through Cross", "[Media][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Sound = MakeSound(*Disc);
	FMoviePlayer Player = MakePlayer(*Disc, *Sound, false);
	REQUIRE(Player.Start());
	CHECK(PlayOut(Player, Cross, 100) == -1);
	CHECK_FALSE(Player.IsFinished());
}

TEST_CASE("A movie stops as it shows its end frame", "[Media][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// Frame 30 shows 29 / 15 s in; the movie stops in the blank that shows it.
	const auto Sound = MakeSound(*Disc);
	FMoviePlayer Player = MakePlayer(*Disc, *Sound, true, 30);
	REQUIRE(Player.Start());
	const int32 Expected = static_cast<int32>(29.0 / 15.0 / PsxTiming::FrameSeconds) + 1;
	const int32 Blanks = PlayOut(Player, NoButtons, 1000);
	CHECK(Blanks >= Expected - 1);
	CHECK(Blanks <= Expected + 1);
	CHECK(Player.PictureNumber() == 30);
}

TEST_CASE("A movie's sound enters the CD input, faded in after its first picture and out before its end", "[Media][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Sound = MakeSound(*Disc);
	// The game's start leaves the CD input at full volume; the program silences it first.
	CHECK(PlayInterrupts(*Sound, 4) == 0x7FFF);
	FMoviePlayer Player = MakePlayer(*Disc, *Sound, true, 30);
	REQUIRE(Player.Start());
	CHECK(Sound->Lock().Spu().QueuedCdInput() > 0);
	CHECK(PlayInterrupts(*Sound, 4) == 0);

	// Once picture 2 shows, the volume rises over 40 steps of two interrupts.
	ShowPicture(Player, 2);
	const int16 Rising = PlayInterrupts(*Sound, 40);
	CHECK(Rising > 0);
	CHECK(Rising < 0x7FFF);
	CHECK(PlayInterrupts(*Sound, 50) == 0x7FFF);

	// From picture 27, three before the end, it falls; the end frame cuts the sound.
	ShowPicture(Player, 27);
	CHECK(PlayInterrupts(*Sound, 20) < 0x7FFF);
	CHECK(PlayOut(Player, NoButtons, 100) > 0);
	CHECK(Sound->Lock().Spu().QueuedCdInput() == 0);
	CHECK(PlayInterrupts(*Sound, 4) == 0);
}

TEST_CASE("A movie played from a start frame passes over the frames before it and one that rewinds starts again at its end", "[Media][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Sound = MakeSound(*Disc);
	FMoviePlayback Playback;
	Playback.StartFrame = 20;
	Playback.EndFrame = 24;
	Playback.bRewinds = true;
	FMoviePlayer Player(Disc->Image(), MovieFifteen(*Disc), Playback, *Sound);
	REQUIRE(Player.Start());
	CHECK(Player.FrameNumber() == 20);
	std::vector<uint32> Frames;
	for (int32 Vblank = 0; Vblank < 40; ++Vblank)
	{
		const auto Playing = Player.Vblank(NoButtons);
		REQUIRE(Playing);
		REQUIRE(*Playing);
		if (Frames.empty() || Frames.back() != Player.FrameNumber())
		{
			Frames.push_back(Player.FrameNumber());
		}
	}
	// Five pictures of 1/15 s fit 20 blanks; after the end frame the movie reads from its start again.
	CHECK(Frames == std::vector<uint32>{20, 21, 22, 23, 24, 20, 21, 22, 23, 24, 20});
	Player.Stop();
	CHECK(Player.IsFinished());
}

TEST_CASE("A silent movie sends no sound to the CD input", "[Media][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Sound = MakeSound(*Disc);
	FMoviePlayback Playback;
	Playback.bSound = false;
	FMoviePlayer Player(Disc->Image(), MovieFifteen(*Disc), Playback, *Sound);
	REQUIRE(Player.Start());
	ShowPicture(Player, 10);
	CHECK(Sound->Lock().Spu().QueuedCdInput() == 0);
}

TEST_CASE("A movie its program skips shows no more pictures and stops 5 vertical blanks later", "[Media][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Sound = MakeSound(*Disc);
	FMoviePlayer Player = MakePlayer(*Disc, *Sound, false);
	REQUIRE(Player.Start());
	ShowPicture(Player, 3);
	const uint32 Shown = Player.PictureNumber();
	Player.Skip();
	CHECK(PlayOut(Player, NoButtons, 100) == FMoviePlayer::SkipVblanks);
	CHECK(Player.PictureNumber() == Shown);
	CHECK(Sound->Lock().Spu().QueuedCdInput() == 0);
}
