#pragma once

#include "Field/EventContext.h"
#include "Sound/SoundFiles.h"

#include <optional>

namespace Weltall
{
	class FCdImage;
	class FFieldMusic;
	class FGameFileIndex;
	class FGameSoundSystem;
	struct FResidentSound;

	/**
	 * The field's songs in the game, as its scripts drive them: the Field program's music, whose loads
	 * finish at the field's poll (at once, without the disc's time), and the sound driver's changes to
	 * the song playing. Game thread only.
	 */
	class FFieldGameSongs final : public IFieldSongs
	{
	public:
		WELTALLCORE_API FFieldGameSongs(FFieldMusic& InMusic, FGameSoundSystem& InSound);

		WELTALLCORE_API void SetSongStart(int32 Song, bool bSilent) override;
		WELTALLCORE_API void ChooseSong(int32 Song) override;
		WELTALLCORE_API bool IsLoading() const override;
		WELTALLCORE_API bool ChangeSong(int32 Song) override;
		WELTALLCORE_API void StartMapSong(bool bAfterDefeat) override;
		WELTALLCORE_API std::optional<FError> Poll() override;
		WELTALLCORE_API int32 ChosenSong() const override;
		WELTALLCORE_API bool IsMusicLoaded() const override;
		WELTALLCORE_API void SetMusicVolume(int32 Volume, int32 Interrupts) override;
		WELTALLCORE_API void SetMusicPitch(int32 Semitones, int32 Interrupts) override;
		WELTALLCORE_API void SetMusicTempo(int32 Scale, int32 Interrupts) override;
		WELTALLCORE_API void SetMusicPan(int32 Offset, int32 Interrupts) override;
		WELTALLCORE_API void MuteMusicTracks(uint32 Mask) override;

	private:
		FFieldMusic& Music;
		FGameSoundSystem& Sound;
	};

	/**
	 * The field's sound effects and bank in the game: the sound driver's effect channels, the bank the
	 * scripts swap into the gear battles' bank's place (the resident sound's), read from the disc at once
	 * and uploaded when the scripts say, and the driver's pause. Game thread only.
	 */
	class FFieldGameSoundEffects final : public IFieldSoundEffects, public IFieldSoundBanks, public IFieldSoundPause
	{
	public:
		WELTALLCORE_API FFieldGameSoundEffects(FGameSoundSystem& InSound, FResidentSound& InResident, const FCdImage& InImage, const FGameFileIndex& InIndex);

		WELTALLCORE_API void StopEffectOn(int32 Channel) override;
		WELTALLCORE_API void PlayEffectOn(uint32 Effect, int32 Channel, int32 Volume, int32 Pan) override;
		WELTALLCORE_API void SetEffectVolumeOn(int32 Channel, int32 Volume) override;
		WELTALLCORE_API void SetEffectPanOn(int32 Channel, int32 Pan) override;
		WELTALLCORE_API void FadeEffectVolumeOn(int32 Channel, int32 Volume, int32 Ticks) override;
		WELTALLCORE_API uint32 PlayingEffects() override;

		WELTALLCORE_API bool BeginBankLoad(int32 Directory, int32 File) override;
		WELTALLCORE_API bool FinishBankLoad() override;

		WELTALLCORE_API void PauseSound() override;
		WELTALLCORE_API void ResumeSound() override;

	private:
		FGameSoundSystem& Sound;
		FResidentSound& Resident;
		const FCdImage& Image;
		const FGameFileIndex& Index;
		/** The bank read, until it is uploaded. */
		std::optional<FSoundBankFile> ReadBank;
	};
}
