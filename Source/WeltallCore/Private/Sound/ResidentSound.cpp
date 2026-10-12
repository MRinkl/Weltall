#include "Sound/ResidentSound.h"

#include "Sound/GameSoundSystem.h"
#include "Sound/SoundDiscFiles.h"

#include <array>

namespace Weltall
{
	namespace
	{
		/** The start-up's banks, files 2-5 of directory 1, in the order it loads them. */
		constexpr int32 BankDirectory = 1;
		constexpr std::array<int32, 4> BankFiles{2, 3, 4, 5};
		constexpr int32 EffectDirectory = 4;
		constexpr int32 MainEffectsFile = 168;
	}

	TResult<std::unique_ptr<FResidentSound>> LoadResidentSound(FGameSoundSystem& Sound, const FCdImage& Image, const FGameFileIndex& Index)
	{
		std::array<FSoundBankFile, BankFiles.size()> Banks;
		for (std::size_t Bank = 0; Bank < BankFiles.size(); ++Bank)
		{
			TResult<FSoundBankFile> File = ReadSoundFile<FSoundBankFile>(Image, Index, BankDirectory, BankFiles[Bank]);
			if (!File)
			{
				return File.Error();
			}
			Banks[Bank] = std::move(*File);
		}
		TResult<FSoundEffectFile> Effects = ReadSoundFile<FSoundEffectFile>(Image, Index, EffectDirectory, MainEffectsFile);
		if (!Effects)
		{
			return Effects.Error();
		}

		auto Resident = std::make_unique<FResidentSound>();
		Resident->MainEffects = std::move(*Effects);
		const FLockedGameSound Locked = Sound.Lock();
		const std::array<const FSoundBank**, BankFiles.size()> Loaded{&Resident->CommonBank, &Resident->BattleThemeBank, &Resident->BattleBank, &Resident->GearBattleBank};
		for (std::size_t Bank = 0; Bank < BankFiles.size(); ++Bank)
		{
			*Loaded[Bank] = Locked->LoadBank(Banks[Bank]);
			if (*Loaded[Bank] == nullptr)
			{
				return MakeError("sound RAM has no room for the bank of file " + std::to_string(BankFiles[Bank]) + " of directory 1");
			}
		}
		return Resident;
	}
}
