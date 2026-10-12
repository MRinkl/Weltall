#pragma once

#include "Foundation/Result.h"
#include "Sound/SoundFiles.h"

#include <memory>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	class FGameSoundSystem;
	struct FSoundBank;

	/**
	 * The sound the game keeps from its start: the four banks its start-up loads, each where it asks
	 * to be in sound RAM, and effect file 0, the effects of the field and the world map.
	 */
	struct FResidentSound
	{
		/** Bank 0: the common effects' instruments. */
		const FSoundBank* CommonBank = nullptr;
		/** Bank 26h: the battle themes' instruments. Some field songs take its room (FFieldMusic puts it back). */
		const FSoundBank* BattleThemeBank = nullptr;
		/** Bank 1: the battles' effects. */
		const FSoundBank* BattleBank = nullptr;
		/** Bank 2: the gear battles' effects. */
		const FSoundBank* GearBattleBank = nullptr;
		/** Effect file 0, registered by the modes that play it for as long as they run. */
		FSoundEffectFile MainEffects;
	};

	/**
	 * Loads the banks into the game's sound as its start-up does (the driver already started), and
	 * reads effect file 0 without registering it. The result stays where it is: the driver keeps
	 * pointers into it.
	 */
	WELTALLCORE_API TResult<std::unique_ptr<FResidentSound>> LoadResidentSound(FGameSoundSystem& Sound, const FCdImage& Image, const FGameFileIndex& Index);
}
