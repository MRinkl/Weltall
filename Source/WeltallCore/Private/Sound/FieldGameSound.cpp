#include "Sound/FieldGameSound.h"

#include "Sound/FieldMusic.h"
#include "Sound/GameSoundSystem.h"
#include "Sound/ResidentSound.h"
#include "Sound/SoundDiscFiles.h"

namespace Weltall
{
	FFieldGameSongs::FFieldGameSongs(FFieldMusic& InMusic, FGameSoundSystem& InSound)
		: Music(InMusic), Sound(InSound)
	{
	}

	void FFieldGameSongs::SetSongStart(int32 Song, bool bSilent)
	{
		Music.SetStart(Song, bSilent ? EFieldSongStart::Silent : EFieldSongStart::FullVolume);
	}

	void FFieldGameSongs::ChooseSong(int32 Song)
	{
		Music.ChooseSong(Song);
	}

	bool FFieldGameSongs::IsLoading() const
	{
		return Music.IsLoading();
	}

	bool FFieldGameSongs::ChangeSong(int32 Song)
	{
		return Music.RequestSong(Song);
	}

	void FFieldGameSongs::StartMapSong(bool bAfterDefeat)
	{
		Music.ChooseEnteringSong(bAfterDefeat);
	}

	std::optional<FError> FFieldGameSongs::Poll()
	{
		const TResult<bool> Loaded = Music.StartChosenSong();
		if (!Loaded)
		{
			return Loaded.Error();
		}
		return std::nullopt;
	}

	int32 FFieldGameSongs::ChosenSong() const
	{
		return Music.Song();
	}

	bool FFieldGameSongs::IsMusicLoaded() const
	{
		return Music.IsLoaded();
	}

	void FFieldGameSongs::SetMusicVolume(int32 Volume, int32 Interrupts)
	{
		if (FSoundSequence* Sequence = Music.Sequence())
		{
			Sound.Lock()->SetSequenceVolume(*Sequence, Volume, Interrupts);
		}
	}

	void FFieldGameSongs::SetMusicPitch(int32 Semitones, int32 Interrupts)
	{
		if (FSoundSequence* Sequence = Music.Sequence())
		{
			Sound.Lock()->SetSequencePitch(*Sequence, Semitones, Interrupts);
		}
	}

	void FFieldGameSongs::SetMusicTempo(int32 Scale, int32 Interrupts)
	{
		if (FSoundSequence* Sequence = Music.Sequence())
		{
			Sound.Lock()->SetTempoScale(*Sequence, Scale, Interrupts);
		}
	}

	void FFieldGameSongs::SetMusicPan(int32 Offset, int32 Interrupts)
	{
		if (FSoundSequence* Sequence = Music.Sequence())
		{
			Sound.Lock()->SetSequencePan(*Sequence, Offset, Interrupts);
		}
	}

	void FFieldGameSongs::MuteMusicTracks(uint32 Mask)
	{
		if (FSoundSequence* Sequence = Music.Sequence())
		{
			Sound.Lock()->MuteTracks(*Sequence, Mask);
		}
	}

	FFieldGameSoundEffects::FFieldGameSoundEffects(FGameSoundSystem& InSound, FResidentSound& InResident, const FCdImage& InImage, const FGameFileIndex& InIndex)
		: Sound(InSound), Resident(InResident), Image(InImage), Index(InIndex)
	{
	}

	void FFieldGameSoundEffects::StopEffectOn(int32 Channel)
	{
		Sound.Lock()->StopEffectOn(Channel);
	}

	void FFieldGameSoundEffects::PlayEffectOn(uint32 Effect, int32 Channel, int32 Volume, int32 Pan)
	{
		Sound.Lock()->PlayEffectOnVolumePan(Effect, Channel, Volume, Pan);
	}

	void FFieldGameSoundEffects::SetEffectVolumeOn(int32 Channel, int32 Volume)
	{
		Sound.Lock()->SetEffectVolumeOn(Channel, Volume);
	}

	void FFieldGameSoundEffects::SetEffectPanOn(int32 Channel, int32 Pan)
	{
		Sound.Lock()->SetEffectPanOn(Channel, Pan);
	}

	void FFieldGameSoundEffects::FadeEffectVolumeOn(int32 Channel, int32 Volume, int32 Ticks)
	{
		Sound.Lock()->FadeEffectVolumeOn(Channel, Volume, Ticks);
	}

	uint32 FFieldGameSoundEffects::PlayingEffects()
	{
		return Sound.Lock()->PlayingEffects(FSoundDriver::AnyEffect);
	}

	bool FFieldGameSoundEffects::BeginBankLoad(int32 Directory, int32 File)
	{
		if (Resident.GearBattleBank != nullptr)
		{
			Sound.Lock()->UnloadBank(*Resident.GearBattleBank);
			Resident.GearBattleBank = nullptr;
		}
		TResult<FSoundBankFile> Read = ReadSoundFile<FSoundBankFile>(Image, Index, Directory, File);
		if (!Read)
		{
			ReadBank.reset();
			return false;
		}
		ReadBank = std::move(*Read);
		return true;
	}

	void FFieldGameSoundEffects::PauseSound()
	{
		Sound.Lock()->Pause();
	}

	void FFieldGameSoundEffects::ResumeSound()
	{
		Sound.Lock()->Resume();
	}

	bool FFieldGameSoundEffects::FinishBankLoad()
	{
		if (!ReadBank)
		{
			return false;
		}
		Resident.GearBattleBank = Sound.Lock()->LoadBank(*ReadBank);
		ReadBank.reset();
		return Resident.GearBattleBank != nullptr;
	}
}
