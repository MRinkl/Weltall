#include "Menu/MenuGameSound.h"

#include "Sound/GameSoundSystem.h"
#include "Sound/SoundFiles.h"

namespace Weltall
{
	FMenuGameSound::FMenuGameSound(FGameSoundSystem& InSound, const FSoundEffectFile& InEffects)
		: Sound(InSound), Effects(InEffects)
	{
	}

	void FMenuGameSound::PlaySystemEffect(uint8 Effect)
	{
		Sound.Lock()->PlaySystemEffect(static_cast<uint32>(Effects.FileId()) << 16 | Effect);
	}

	void FMenuGameSound::Pause()
	{
		Sound.Lock()->Pause();
	}

	void FMenuGameSound::Resume()
	{
		Sound.Lock()->Resume();
	}

	int32 FMenuGameSound::OutputMode() const
	{
		return Sound.Lock()->OutputMode();
	}

	void FMenuGameSound::SetOutputMode(int32 Mode)
	{
		Sound.Lock()->SetOutputMode(Mode);
	}
}
