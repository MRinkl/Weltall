#include "Battling/BattlingSound.h"

#include "Game/PsxRandom.h"

namespace Weltall
{
	namespace
	{
		constexpr int16 FullVolume = 0x7F;
		constexpr int16 MiddlePan = 0x40;
		constexpr int32 ScreenWidth = 0x140;
		constexpr int32 FarDepth = 0x3000;
		constexpr int32 QuietestVolume = 0x28;
		constexpr uint8 NoSound = 0xFF;
		constexpr uint32 FirstVoiceMask = 0x300;

		uint8 SideTag(int32 Side, uint32 Effect)
		{
			return static_cast<uint8>((Effect & 0x7F) | (Side != 0 ? 0x80 : 0));
		}
	}

	FBattlingSoundChannels::FBattlingSoundChannels(IBattlingSoundDriver& InDriver)
		: Driver(InDriver)
	{
		for (int32 Index = 0; Index < ChannelCount; ++Index)
		{
			Channels[static_cast<std::size_t>(Index)].Voice = static_cast<uint8>(2 * Index);
			Channels[static_cast<std::size_t>(Index)].VoiceMask = FirstVoiceMask << (2 * Index);
		}
	}

	void FBattlingSoundChannels::Reset()
	{
		Driver.StopAll();
		for (int32 Index = 0; Index < ChannelCount; ++Index)
		{
			FChannel& Channel = Channels[static_cast<std::size_t>(Index)];
			Channel.VoiceMask = FirstVoiceMask << (2 * Index);
			Channel.bActive = false;
			Channel.Age = 0;
			Channel.Voice = static_cast<uint8>(2 * Index);
		}
	}

	void FBattlingSoundChannels::Age()
	{
		for (FChannel& Channel : Channels)
		{
			if (Channel.Age != 0xFFFF)
			{
				++Channel.Age;
			}
		}
	}

	void FBattlingSoundChannels::Start(uint32 Effect, EBattlingSoundPlace Place, const FBattlingPoint* Point, uint8 Tag)
	{
		// A free channel, or for a centred sound one already playing a centred sound; else the one started longest ago.
		FChannel* Chosen = &Channels[ChannelCount - 1];
		uint16 Oldest = 0;
		for (FChannel& Channel : Channels)
		{
			if (!Channel.bActive || (Place == EBattlingSoundPlace::Centred && Channel.Place == EBattlingSoundPlace::Centred))
			{
				Chosen = &Channel;
				break;
			}
			if (Oldest < Channel.Age)
			{
				Chosen = &Channel;
				Oldest = Channel.Age;
			}
		}
		Age();
		Chosen->Place = Place;
		Chosen->Effect = Effect;
		Chosen->bActive = true;
		Chosen->Tag = Tag;
		Chosen->Following = Point;
		if (Point != nullptr)
		{
			Chosen->Point = *Point;
		}
		Chosen->Age = 0;
		if (Place == EBattlingSoundPlace::Centred)
		{
			Driver.PlayEffect(Chosen->Effect, Chosen->Voice, FullVolume, MiddlePan);
		}
	}

	void FBattlingSoundChannels::Play(int32 Effect, uint32 VBlank)
	{
		if (Effect != 0)
		{
			Start(EffectBase + static_cast<uint32>(Effect), EBattlingSoundPlace::Centred, nullptr, static_cast<uint8>(VBlank));
		}
	}

	void FBattlingSoundChannels::PlayAt(int32 Side, int32 Effect, const FBattlingPoint* Point, EBattlingSoundPlace Place)
	{
		if (Effect != 0)
		{
			Start(EffectBase + static_cast<uint32>(Effect), Place, Point, SideTag(Side, static_cast<uint32>(Effect)));
		}
	}

	void FBattlingSoundChannels::PlayFighter(const FBattlingProgramTables& Tables, const FBattlingSoundSet& Set, int32 Side, int32 Index,
		const FBattlingPoint* Point, EBattlingSoundPlace Place, FPsxRandom& Random)
	{
		if (Index == 0)
		{
			Index = Random.Next() % 6 + 1;
		}
		const uint8 Entry = Set[static_cast<std::size_t>(Index)];
		if (Entry == NoSound)
		{
			return;
		}
		for (const uint8 Effect : Tables.SoundPairs[Entry])
		{
			if (Effect != 0)
			{
				Start(Effect | EffectBase, Place, Point, SideTag(Side, Effect));
			}
		}
	}

	void FBattlingSoundChannels::StopTag(uint8 Tag)
	{
		for (FChannel& Channel : Channels)
		{
			if (Channel.bActive && Channel.Tag == Tag)
			{
				Driver.StopVoice(Channel.Voice);
				Channel.bActive = false;
			}
		}
	}

	void FBattlingSoundChannels::StopFighter(const FBattlingProgramTables& Tables, const FBattlingSoundSet& Set, int32 Side, int32 Index)
	{
		const uint8 Entry = Set[static_cast<std::size_t>(Index)];
		if (Entry == NoSound)
		{
			return;
		}
		for (const uint8 Effect : Tables.SoundPairs[Entry])
		{
			if (Effect != 0)
			{
				StopTag(SideTag(Side, Effect));
			}
		}
	}

	void FBattlingSoundChannels::Poll()
	{
		for (FChannel& Channel : Channels)
		{
			if ((Driver.PlayingVoices(Channel.Effect) & Channel.VoiceMask) == 0)
			{
				Channel.bActive = false;
			}
		}
		Age();
	}

	void FBattlingSoundChannels::UpdatePositions(const FGte& View, const FBattlingPoint& Origin)
	{
		for (FChannel& Channel : Channels)
		{
			if (!Channel.bActive || Channel.Place == EBattlingSoundPlace::Centred)
			{
				continue;
			}
			const FBattlingPoint& Point = Channel.Place == EBattlingSoundPlace::Fixed ? Channel.Point : *Channel.Following;
			const FGteVector Relative{static_cast<int16>(Point[0] - Origin[0]), static_cast<int16>(Point[1] - Origin[1]),
				static_cast<int16>(Point[2] - Origin[2])};
			const FGteProjection Projection = View.Rtps(Relative);
			int32 ScreenX = Projection.ScreenX;
			if (ScreenX < 0)
			{
				ScreenX = 0;
			}
			if (ScreenX > ScreenWidth)
			{
				ScreenX = ScreenWidth;
			}
			const int32 Loudness = (FarDepth - static_cast<int32>(Projection.ScreenZ)) * FullVolume / FarDepth;
			int16 Volume = static_cast<int16>(Loudness < QuietestVolume ? QuietestVolume : Loudness);
			if (Loudness > FullVolume)
			{
				Volume = FullVolume;
			}
			const int16 Pan = static_cast<int16>(ScreenX * FullVolume / ScreenWidth);
			if (Channel.Age == 0)
			{
				Driver.PlayEffect(Channel.Effect, Channel.Voice, Volume, Pan);
			}
			else
			{
				Driver.SetPan(Channel.Voice, Pan);
				Driver.SetVolume(Channel.Voice, Volume);
			}
		}
		Age();
	}
}
