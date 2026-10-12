#include "EventOps.h"

namespace Weltall::EventOps
{
	namespace
	{
		constexpr int32 FullVolume = 0x7F;
		constexpr int32 CentrePan = 0x40;
		/** The channel 74 and FE 5D play on. */
		constexpr int32 SoundChannel = 3;
		/** The scripts' channels a sound driver's effect player has: 16 tracks, two a channel. */
		constexpr int32 ScriptChannels = 8;

		/** The sound driver's channel for a script's: each of the scripts' eight takes a pair. */
		int32 DriverChannel(int32 Channel)
		{
			return (Channel & 7) << 1;
		}

		/** Stops what plays on the channel and starts Effect there; volume and pan keep their low 16 bits, signed. */
		void PlayOn(FEventContext& Context, int32 Effect, int32 Volume, int32 Pan, int32 Channel)
		{
			if (Context.SoundEffects != nullptr)
			{
				Context.SoundEffects->StopEffectOn(DriverChannel(Channel));
				Context.SoundEffects->PlayEffectOn(static_cast<uint32>(Effect), DriverChannel(Channel), static_cast<int16>(Volume), static_cast<int16>(Pan));
			}
		}

		/**
		 * The channels past the eight that the game's scripts name (FE 62's 30 on map 272, FE 8C's 12 on map 670). Their
		 * pairs lie past the effect player, in driver memory that holds no playing track wherever the scripts reach
		 * them, so the driver changes nothing.
		 */
		constexpr int32 StrayChannels[] = {12, 30};

		/**
		 * Applies a change to the pair of an unmasked script channel, which FE 62, FE 63 and FE 8C take; false
		 * after a fault. Past the eight the pair lies outside the effect player: the stray channels change nothing,
		 * any other is a fault.
		 */
		template <typename FChange>
		bool ChangeUnmaskedChannel(FEventContext& Context, int32 Channel, FChange&& Change)
		{
			const int32 Masked = Channel & 0x7F;
			if (Masked < ScriptChannels)
			{
				if (Context.SoundEffects != nullptr)
				{
					Change(*Context.SoundEffects, Channel << 1);
				}
				return true;
			}
			for (const int32 Stray : StrayChannels)
			{
				if (Masked == Stray)
				{
					return true;
				}
			}
			Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Channel));
			return false;
		}

		/** 74: an effect on the scripts' channel 3, or 0 to stop it. */
		void PlaySound(FEventContext& Context)
		{
			PlayScriptSound(Context, ReadValue(Context, 1), SoundChannel);
			Advance(Context, 3);
		}

		/** FE 65: an effect on a channel, or 0 to stop it. */
		void PlaySoundOn(FEventContext& Context)
		{
			const int32 Effect = ReadValue(Context, 1);
			PlayScriptSound(Context, Effect, ReadValue(Context, 3));
			Advance(Context, 5);
		}

		/** FE 66: an effect on a channel at a volume and pan; effect 0 is played too. */
		void PlaySoundOnVolumePan(FEventContext& Context)
		{
			const int32 Effect = ReadValue(Context, 1);
			const int32 Volume = ReadValue(Context, 5);
			const int32 Pan = ReadValue(Context, 3);
			PlayOn(Context, Effect, Volume, Pan, ReadValue(Context, 7));
			Advance(Context, 9);
		}

		/** FE 5D: an effect on the scripts' channel 3 at a pan and volume; effect 0 is played too. */
		void PlaySoundPanVolume(FEventContext& Context)
		{
			const int32 Effect = ReadValue(Context, 1);
			const int32 Volume = ReadValue(Context, 5);
			const int32 Pan = ReadValue(Context, 3);
			PlayOn(Context, Effect, Volume, Pan, SoundChannel);
			Advance(Context, 7);
		}

		/** FE 62: the volume of what plays on a channel. */
		void SetChannelVolume(FEventContext& Context)
		{
			const int32 Channel = ReadValue(Context, 3);
			const int32 Volume = ReadValue(Context, 1);
			if (ChangeUnmaskedChannel(Context, Channel, [Volume](IFieldSoundEffects& Sound, int32 Driver) { Sound.SetEffectVolumeOn(Driver, Volume); }))
			{
				Advance(Context, 5);
			}
		}

		/** FE 63: the pan of what plays on a channel. */
		void SetChannelPan(FEventContext& Context)
		{
			const int32 Channel = ReadValue(Context, 3);
			const int32 Pan = ReadValue(Context, 1);
			if (ChangeUnmaskedChannel(Context, Channel, [Pan](IFieldSoundEffects& Sound, int32 Driver) { Sound.SetEffectPanOn(Driver, Pan); }))
			{
				Advance(Context, 5);
			}
		}

		/** FE 8C: what plays on a channel slides to a volume over some of its own ticks. */
		void FadeChannel(FEventContext& Context)
		{
			const int32 Channel = ReadValue(Context, 3);
			const int32 Volume = ReadValue(Context, 1);
			const int32 Ticks = ReadValue(Context, 5);
			if (ChangeUnmaskedChannel(Context, Channel, [Volume, Ticks](IFieldSoundEffects& Sound, int32 Driver) { Sound.FadeEffectVolumeOn(Driver, Volume, Ticks); }))
			{
				Advance(Context, 7);
			}
		}

		/**
		 * FE 64: waits on its first byte while any of the effect player's tracks 8 to 15 its mask selects
		 * plays (bit n: track 8 + n, so the scripts' channels 0 to 3 as bit pairs); the turn ends either way.
		 */
		void WaitChannels(FEventContext& Context)
		{
			const uint32 Playing = Context.SoundEffects != nullptr ? Context.SoundEffects->PlayingEffects() : 0;
			const int32 Mask = ReadValue(Context, 1);
			Advance(Context, (Playing & static_cast<uint32>(Mask) << 8) == 0 ? 3 : -1);
			Context.Run.bYield = true;
		}

		/** FE 8A: who hears the actors' positional sounds. */
		void SetSoundListener(FEventContext& Context)
		{
			Context.ActorSounds.Listener = static_cast<uint16>(ReadValue(Context, 1));
			Advance(Context, 3);
		}

		/** FE 8D: the channels a map change leaves playing. */
		void SetSoundKeepMask(FEventContext& Context)
		{
			Context.ActorSounds.KeepMask = static_cast<uint16>(ReadValue(Context, 1));
			Advance(Context, 3);
		}

		/** FE 98: how far the actors' positional sounds carry. */
		void SetSoundRange(FEventContext& Context)
		{
			Context.ActorSounds.Range = static_cast<int16>(ReadValue(Context, 1));
			Advance(Context, 3);
		}
	}

	void PlayScriptSound(FEventContext& Context, int32 Effect, int32 Channel)
	{
		if (Effect != 0)
		{
			PlayOn(Context, Effect, FullVolume, CentrePan, Channel);
		}
		else if (Context.SoundEffects != nullptr)
		{
			Context.SoundEffects->StopEffectOn(DriverChannel(Channel));
		}
	}

	void RegisterSoundEffects(FBaseTable& Base, FExtendedTable& Extended)
	{
		Base[0x74] = &PlaySound;
		Extended[0x5D] = &PlaySoundPanVolume;
		Extended[0x62] = &SetChannelVolume;
		Extended[0x63] = &SetChannelPan;
		Extended[0x64] = &WaitChannels;
		Extended[0x65] = &PlaySoundOn;
		Extended[0x66] = &PlaySoundOnVolumePan;
		Extended[0x8A] = &SetSoundListener;
		Extended[0x8C] = &FadeChannel;
		Extended[0x8D] = &SetSoundKeepMask;
		Extended[0x98] = &SetSoundRange;
	}
}
