#pragma once

#include "Menu/MenuProgram.h"

namespace Weltall
{
	class FGameSoundSystem;
	class FSoundEffectFile;

	/**
	 * The menu's sound in the game: its system effects are the field's resident effect file's (effect
	 * file 0, which the field registers while it runs), played on the driver's system channels, and the
	 * output mode is the driver's. Game thread only.
	 */
	class FMenuGameSound final : public IMenuSound
	{
	public:
		WELTALLCORE_API FMenuGameSound(FGameSoundSystem& InSound, const FSoundEffectFile& InEffects);

		WELTALLCORE_API void PlaySystemEffect(uint8 Effect) override;
		WELTALLCORE_API void Pause() override;
		WELTALLCORE_API void Resume() override;
		WELTALLCORE_API int32 OutputMode() const override;
		WELTALLCORE_API void SetOutputMode(int32 Mode) override;

	private:
		FGameSoundSystem& Sound;
		const FSoundEffectFile& Effects;
	};
}
