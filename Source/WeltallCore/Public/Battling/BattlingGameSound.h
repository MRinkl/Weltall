#pragma once

#include "Battling/BattlingSound.h"

namespace Weltall
{
	class FGameSoundSystem;
	class FSoundEffectFile;
	struct FSoundSequence;

	/**
	 * Battling's sound in the game: its channels' calls on the session's sound driver, the program's effects registered while
	 * it lives, and the song the field left playing. Game thread only.
	 */
	class FBattlingGameSound final : public IBattlingSoundDriver
	{
	public:
		/** Music: the field's song, none when nothing plays. Everything must outlive it. */
		WELTALLCORE_API FBattlingGameSound(FGameSoundSystem& Sound, const FSoundEffectFile& Effects, FSoundSequence* Music);
		WELTALLCORE_API ~FBattlingGameSound() override;
		FBattlingGameSound(const FBattlingGameSound&) = delete;
		FBattlingGameSound& operator=(const FBattlingGameSound&) = delete;

		/** Whether the driver took the program's effects. */
		bool HasEffects() const { return bRegistered; }

		WELTALLCORE_API void PlayEffect(uint32 Effect, uint8 Voice, int16 Volume, int16 Pan) override;
		WELTALLCORE_API void SetPan(uint8 Voice, int16 Pan) override;
		WELTALLCORE_API void SetVolume(uint8 Voice, int16 Volume) override;
		WELTALLCORE_API void StopVoice(uint8 Voice) override;
		WELTALLCORE_API void StopAll() override;
		WELTALLCORE_API void StopSong() override;
		WELTALLCORE_API void SetSongTempo(int32 Scale) override;
		WELTALLCORE_API uint32 PlayingVoices(uint32 Effect) override;

	private:
		FGameSoundSystem& Sound;
		const FSoundEffectFile& Effects;
		FSoundSequence* Music = nullptr;
		bool bRegistered = false;
	};
}
