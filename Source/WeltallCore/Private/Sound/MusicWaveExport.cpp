#include "Sound/MusicWaveExport.h"

#include "Sound/GameSoundSession.h"
#include "Sound/SoundDriverState.h"

#include <algorithm>
#include <array>

namespace Weltall
{
	namespace
	{
		constexpr uint32 MaxSongFrames = 44100 * 10 * 60;
		constexpr uint32 ReleaseTailFrames = 44100 * 2;
	}

	TResult<std::unique_ptr<FMusicWaveExport>> FMusicWaveExport::Start(FGameSoundSession& Session, int32 Song)
	{
		{
			const FLockedGameSound Locked = Session.Sound()->Lock();
			if (Locked->FindEffects(Session.Resident().MainEffects.FileId()) == nullptr
				&& !Locked->RegisterEffects(Session.Resident().MainEffects))
			{
				return MakeError("Music export cannot register the resident sound effects.");
			}
		}
		Session.FieldMusic().RequestSong(FFieldMusic::NoSong);
		const TResult<bool> Started = Session.FieldMusic().ChangeSong(Song, EFieldSongStart::FullVolume);
		if (!Started) return Started.Error();
		if (!*Started || Session.FieldMusic().Sequence() == nullptr) return MakeError("The selected song has no playable sequence.");
		return std::unique_ptr<FMusicWaveExport>(new FMusicWaveExport(Session));
	}

	TResult<bool> FMusicWaveExport::Step(uint32 MaxFrames)
	{
		if (bDone) return true;
		std::array<int16, 2048> Buffer{};
		while (MaxFrames != 0)
		{
			const uint32 Frames = std::min<uint32>(MaxFrames, static_cast<uint32>(Buffer.size() / 2));
			if (Output.Frames() + Frames > MaxSongFrames) return MakeError("Music export reached ten minutes without a complete loop or ending.");
			Session.Sound()->Render(std::span<int16>(Buffer).first(Frames * 2));
			Output.Samples.insert(Output.Samples.end(), Buffer.begin(), Buffer.begin() + Frames * 2);
			MaxFrames -= Frames;
			const FLockedGameSound Locked = Session.Sound()->Lock();
			const FSoundSequence* Sequence = Session.FieldMusic().Sequence();
			if (Sequence == nullptr) return MakeError("The song was unloaded during its export.");
			if (!Sequence->IsPlaying())
			{
				if (Output.Frames() >= Sequence->PlaybackFrames + ReleaseTailFrames)
				{
					Output.Samples.resize(static_cast<std::size_t>(Sequence->PlaybackFrames + ReleaseTailFrames) * 2);
					bDone = true;
				}
			}
			else
			{
				bool bLooped = Sequence->Active != 0;
				const FSoundTrack* LastLoop = nullptr;
				for (std::size_t Track = 0; Track < Sequence->TrackCount; ++Track)
				{
					const FSoundTrack& State = Sequence->Tracks[Track];
					if (State.Flags == 0) continue;
					if (State.Loops == 0) bLooped = false;
					if (State.Loops != 0 && (LastLoop == nullptr || State.FirstLoopEndFrame > LastLoop->FirstLoopEndFrame)) LastLoop = &State;
				}
				if (bLooped && LastLoop != nullptr && LastLoop->FirstLoopFrame < LastLoop->FirstLoopEndFrame)
				{
					Output.Loop = FWaveLoop{static_cast<uint32>(LastLoop->FirstLoopFrame), static_cast<uint32>(LastLoop->FirstLoopEndFrame)};
					Output.Samples.resize(static_cast<std::size_t>(Output.Loop->EndFrame) * 2);
					bDone = true;
				}
			}
			if (bDone) return true;
		}
		return false;
	}
}
