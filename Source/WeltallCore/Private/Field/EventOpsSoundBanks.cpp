#include "EventOps.h"

namespace Weltall::EventOps
{
	namespace
	{
		constexpr uint8 FinishMode = 1;
		/** The slot of the bank the scripts swap: the gear battles' effects. */
		constexpr int32 GearBattleSlot = 3;
		/** A selector's bit 7 names a bank of the battles' voices, otherwise one of the music's directory. */
		constexpr int32 VoiceBankBit = 0x80;
		constexpr int32 MusicDirectory = 28;
		constexpr int32 FirstMusicBank = 2;
		constexpr int32 VoiceDirectory = 45;
		constexpr int32 FirstVoiceBank = 0x1F;

		/**
		 * FE B0: a sound bank swapped into the gear battles' bank's place, in two instructions. The start
		 * (any mode but 1) gives that bank up, reads the selected bank file and goes on; the finish (mode
		 * 1) uploads the bank and ends the turn. Both wait while the disc reads, which the port's does at
		 * once.
		 */
		void LoadSoundBank(FEventContext& Context)
		{
			if (CodeByte(Context, 1) == FinishMode)
			{
				if (Context.SoundBanks != nullptr && !Context.SoundBanks->FinishBankLoad())
				{
					Fail(Context, EEventFault::SoundBankNotLoaded, 0);
					return;
				}
				Context.Run.bYield = true;
				Advance(Context, 2);
				return;
			}
			const int32 Slot = ReadValue(Context, 2);
			if (Slot != GearBattleSlot)
			{
				// Carried past, no bank is read.
				if (Recovers(Context, "a sound bank outside the gear battles' slot", EEventFault::SoundBankSlot, static_cast<uint32>(Slot)))
				{
					Advance(Context, 6);
				}
				return;
			}
			const int32 Selector = ReadValue(Context, 4);
			const bool bVoices = (Selector & VoiceBankBit) != 0;
			const int32 Directory = bVoices ? VoiceDirectory : MusicDirectory;
			const int32 File = bVoices ? (Selector & 0x7F) + FirstVoiceBank : Selector + FirstMusicBank;
			if (Context.SoundBanks != nullptr && !Context.SoundBanks->BeginBankLoad(Directory, File))
			{
				Fail(Context, EEventFault::SoundBankUnreadable, static_cast<uint32>(Directory) << 16 | static_cast<uint16>(File));
				return;
			}
			Advance(Context, 6);
		}
	}

	void RegisterSoundBanks(FBaseTable&, FExtendedTable& Extended)
	{
		Extended[0xB0] = &LoadSoundBank;
	}
}
