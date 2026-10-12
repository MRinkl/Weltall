#include "Battling/BattlingGameSound.h"

#include "Sound/GameSoundSystem.h"
#include "Sound/SoundFiles.h"

namespace Weltall
{
	FBattlingGameSound::FBattlingGameSound(FGameSoundSystem& InSound, const FSoundEffectFile& InEffects, FSoundSequence* InMusic)
		: Sound(InSound), Effects(InEffects), Music(InMusic)
	{
		bRegistered = Sound.Lock()->RegisterEffects(Effects);
	}

	FBattlingGameSound::~FBattlingGameSound()
	{
		if (bRegistered)
		{
			Sound.Lock()->UnregisterEffects(Effects);
		}
	}

	void FBattlingGameSound::PlayEffect(uint32 Effect, uint8 Voice, int16 Volume, int16 Pan)
	{
		Sound.Lock()->PlayEffectOnVolumePan(Effect, Voice, Volume, Pan);
	}

	void FBattlingGameSound::SetPan(uint8 Voice, int16 Pan)
	{
		Sound.Lock()->SetEffectPanOn(Voice, Pan);
	}

	void FBattlingGameSound::SetVolume(uint8 Voice, int16 Volume)
	{
		Sound.Lock()->SetEffectVolumeOn(Voice, Volume);
	}

	void FBattlingGameSound::StopVoice(uint8 Voice)
	{
		Sound.Lock()->StopEffectOn(Voice);
	}

	void FBattlingGameSound::StopAll()
	{
		Sound.Lock()->StopAllEffects();
	}

	void FBattlingGameSound::StopSong()
	{
		if (Music != nullptr)
		{
			Sound.Lock()->StopSequence(*Music);
		}
	}

	void FBattlingGameSound::SetSongTempo(int32 Scale)
	{
		if (Music != nullptr)
		{
			Sound.Lock()->SetTempoScale(*Music, Scale, 0);
		}
	}

	uint32 FBattlingGameSound::PlayingVoices(uint32 Effect)
	{
		return Sound.Lock()->PlayingEffects(Effect);
	}
}
