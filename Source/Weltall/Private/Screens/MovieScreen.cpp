#include "Screens/MovieScreen.h"

#include "Media/GameMovies.h"
#include "Media/Iso9660.h"
#include "Media/PsxExecutable.h"
#include "Timing/PsxTiming.h"
#include "WeltallLog.h"

TSharedPtr<FMovieScreen> FMovieScreen::Open(const Weltall::FIso9660& Disc, int32 Number, uint32 EndFrame, bool bSkippable, Weltall::FGameSoundSystem& Sound,
	Weltall::EMovieFilter Filter)
{
	const auto Executable = Weltall::ReadBootExecutable(Disc);
	if (!Executable)
	{
		UE_LOG(LogWeltall, Error, TEXT("The disc's program cannot be read: %hs"), Executable.Error().Message.c_str());
		return nullptr;
	}
	const auto Index = Weltall::FGameFileIndex::Read(*Executable);
	if (!Index)
	{
		UE_LOG(LogWeltall, Error, TEXT("The disc's file index cannot be read: %hs"), Index.Error().Message.c_str());
		return nullptr;
	}
	const std::optional<Weltall::FMovieLocation> Movie = Weltall::FindMovie(*Index, Number);
	if (!Movie)
	{
		return nullptr;
	}
	TSharedRef<FMovieScreen> Opened = MakeShared<FMovieScreen>(Disc.Image(), *Movie, EndFrame, bSkippable, Sound, Filter);
	// With the first picture decoded and the sound read ahead of it, the sound starts with the picture.
	const Weltall::TResult<bool> Started = Opened->Player.Start();
	if (!Started)
	{
		UE_LOG(LogWeltall, Error, TEXT("The movie cannot start: %hs"), Started.Error().Message.c_str());
		return nullptr;
	}
	Opened->Picture.Follow(Opened->Player, Filter);
	return Opened;
}

FMovieScreen::FMovieScreen(const Weltall::FCdImage& Image, Weltall::FMovieLocation Location, uint32 EndFrame, bool bSkippable, Weltall::FGameSoundSystem& Sound,
	Weltall::EMovieFilter InFilter)
	: Player(Image, Location, {.EndFrame = EndFrame, .bSkippable = bSkippable}, Sound), Vblanks(Weltall::PsxTiming::FrameSeconds), Filter(InFilter)
{
}

void FMovieScreen::Update(double DeltaSeconds, Weltall::FPadState Held)
{
	if (Player.IsFinished())
	{
		return;
	}
	Latch.Sample(Held);
	const int32 Due = Vblanks.Advance(DeltaSeconds);
	for (int32 Vblank = 0; Vblank < Due && !Player.IsFinished(); ++Vblank)
	{
		const Weltall::TResult<bool> Playing = Player.Vblank(Latch.TakeForTick());
		if (!Playing)
		{
			UE_LOG(LogWeltall, Error, TEXT("The movie stopped: %hs"), Playing.Error().Message.c_str());
		}
	}
	Picture.Follow(Player, Filter);
}

TSharedRef<const IScreenFrame> FMovieScreen::CaptureFrame() const
{
	return Picture.Capture(Screen, Window);
}
