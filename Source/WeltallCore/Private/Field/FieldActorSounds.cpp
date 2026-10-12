#include "Field/FieldActorSounds.h"

#include "Field/EventContext.h"
#include "Field/FieldCamera.h"
#include "Foundation/ProcessorDivide.h"
#include "Graphics/Gte.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"

#include <string>

namespace Weltall
{
	namespace
	{
		constexpr int32 NoSound = -1;
		/** The nearest three start this far away: a sound farther off is never heard. */
		constexpr int32 OutOfEarshot = 0xFFFF;
		constexpr int32 NoChannel = -1;
		constexpr int32 ScreenWidth = 0x140;
		/** The pan per pixel across the screen, 16.16: 0x7F at its right edge. */
		constexpr int32 PanPerPixel = 0x6666;

		/** One of the three sounds heard this tick. */
		struct FHeardSound
		{
			int32 Distance = OutOfEarshot;
			int32 Sound = NoSound;
			int32 Volume = 0;
			int32 Actor = 0;
			bool bPlaying = false;
		};

		int32 DriverChannel(int32 Channel)
		{
			return Channel << 1;
		}

		int16 Whole(int32 Value)
		{
			return static_cast<int16>(Value >> 16);
		}

		/**
		 * The volume of a sound Volume loud at Distance: all of it close by, falling in the original's
		 * integer steps to a 127th of it at the range and beyond. The range is not checked: 0 divides as
		 * the processor does. A sound playing on gets the whole word, a sound starting its low half.
		 */
		int32 VolumeAt(int32 Distance, int16 Range, int32 Volume)
		{
			const int32 Near = Range < Distance ? Range : Distance;
			const int32 Faded = static_cast<int32>(static_cast<uint32>(ProcessorDivide(0x7F0000, Range)) * static_cast<uint32>(Near)) >> 16;
			const uint32 Scale = (static_cast<uint32>(0x80 - Faded) << 16) / 0x7F;
			return static_cast<int32>((Scale * static_cast<uint32>(Volume)) >> 16);
		}

		/** The pan of a sound whose actor's origin falls at ScreenX: left edge 0, right edge 0x7F. */
		int16 PanAt(int32 ScreenX)
		{
			int32 X = ScreenX;
			if (X > ScreenWidth)
			{
				X = ScreenWidth - 1;
			}
			if (X < 0)
			{
				X = 0;
			}
			return static_cast<int16>((X * PanPerPixel) >> 16);
		}

		/** Where the actor's origin falls across the screen: its model's placement through the view, projected. */
		int32 ScreenX(const FEventContext& Context, int32 Actor)
		{
			const FFieldActor& Record = Context.Actors[static_cast<std::size_t>(Actor)];
			FGteMatrix Placement;
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				Placement.M[Row] = Record.Matrix[Row];
			}
			Placement.T = Record.Translation;
			FGte Gte;
			Gte.Transform = CompMatrix(Context.Camera.View.View, Placement);
			Gte.OffsetX = FieldScreenCenterX << 16;
			Gte.OffsetY = FieldScreenCenterY << 16;
			Gte.ProjectionDistance = Context.ScriptView.ProjectionDistance;
			return Gte.Rtps({0, 0, 0}).ScreenX;
		}

		/** The point the sounds are heard from, whole units; false when the listener is none of the three. */
		bool FindListener(const FEventContext& Context, std::array<int16, 3>& Point, bool& bLeaderWithoutState)
		{
			const FFieldPoint* Camera = nullptr;
			switch (Context.ActorSounds.Listener)
			{
			case FieldSoundListener::Leader:
				if (Context.LeaderActor >= Context.States.size())
				{
					bLeaderWithoutState = true;
					return false;
				}
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Point[Axis] = Whole(Context.States[Context.LeaderActor].Position[Axis]);
				}
				return true;
			case FieldSoundListener::CameraEye:
				Camera = &Context.Camera.Eye;
				break;
			case FieldSoundListener::CameraTarget:
				Camera = &Context.Camera.Target;
				break;
			default:
				return false;
			}
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Point[Axis] = Whole((*Camera)[Axis]);
			}
			return true;
		}

		/** The channel the actor holds, for a sound heard; NoChannel for none. */
		int32 HeldChannel(const FFieldActorSounds& Sounds, const FHeardSound& Heard)
		{
			if (Heard.Sound == NoSound)
			{
				return NoChannel;
			}
			for (int32 Channel = 0; Channel < FFieldActorSounds::ChannelCount; ++Channel)
			{
				if (Sounds.Channels[static_cast<std::size_t>(Channel)].Owner == static_cast<uint32>(Heard.Actor))
				{
					return Channel;
				}
			}
			return NoChannel;
		}

		/** A sound playing on: every channel its actor holds follows its distance and place on screen. */
		void AdjustSound(FEventContext& Context, const FHeardSound& Heard)
		{
			FFieldActorSounds& Sounds = Context.ActorSounds;
			for (int32 Channel = 0; Channel < FFieldActorSounds::ChannelCount; ++Channel)
			{
				if (Sounds.Channels[static_cast<std::size_t>(Channel)].Owner != static_cast<uint32>(Heard.Actor))
				{
					continue;
				}
				const int32 Volume = VolumeAt(Heard.Distance, Sounds.Range, Heard.Volume);
				const int16 Pan = PanAt(ScreenX(Context, Heard.Actor));
				if (Context.SoundEffects != nullptr)
				{
					Context.SoundEffects->SetEffectVolumeOn(DriverChannel(Channel), Volume);
					Context.SoundEffects->SetEffectPanOn(DriverChannel(Channel), Pan);
				}
			}
		}

		/** A sound newly heard: it takes the first free channel, if one is. */
		void StartSound(FEventContext& Context, const FHeardSound& Heard)
		{
			FFieldActorSounds& Sounds = Context.ActorSounds;
			for (int32 Channel = 0; Channel < FFieldActorSounds::ChannelCount; ++Channel)
			{
				FFieldActorSoundChannel& Held = Sounds.Channels[static_cast<std::size_t>(Channel)];
				if (Held.Sound != FFieldActorSounds::Free)
				{
					continue;
				}
				Held.Sound = static_cast<uint16>(Heard.Sound);
				Held.Owner = static_cast<uint16>(Heard.Actor);
				const int16 Volume = static_cast<int16>(VolumeAt(Heard.Distance, Sounds.Range, Heard.Volume));
				const int16 Pan = PanAt(ScreenX(Context, Heard.Actor));
				if (Context.SoundEffects != nullptr)
				{
					Context.SoundEffects->StopEffectOn(DriverChannel(Channel));
					Context.SoundEffects->PlayEffectOn(static_cast<uint32>(Heard.Sound), DriverChannel(Channel), Volume, Pan);
				}
				return;
			}
		}
	}

	void StopFieldActorSoundsForExit(FEventContext& Context)
	{
		FFieldActorSounds& Sounds = Context.ActorSounds;
		Sounds.Channels.fill({});
		constexpr int32 KeptChannels = 4;
		for (int32 Channel = 0; Channel < KeptChannels; ++Channel)
		{
			if ((Sounds.KeepMask & 1) == 0 && Context.SoundEffects != nullptr)
			{
				Context.SoundEffects->StopEffectOn(DriverChannel(Channel));
			}
			Sounds.KeepMask = static_cast<uint16>(Sounds.KeepMask >> 1);
		}
	}

	void ReleaseActorSound(FEventContext& Context, int32 Actor)
	{
		for (int32 Channel = 0; Channel < FFieldActorSounds::ChannelCount; ++Channel)
		{
			FFieldActorSoundChannel& Held = Context.ActorSounds.Channels[static_cast<std::size_t>(Channel)];
			if (Held.Owner == static_cast<uint32>(Actor))
			{
				if (Context.SoundEffects != nullptr)
				{
					Context.SoundEffects->StopEffectOn(DriverChannel(Channel));
				}
				Held = {};
				return;
			}
		}
	}

	std::optional<FError> UpdateFieldActorSounds(FEventContext& Context)
	{
		std::array<int16, 3> Listener{};
		bool bLeaderWithoutState = false;
		if (!FindListener(Context, Listener, bLeaderWithoutState))
		{
			if (bLeaderWithoutState)
			{
				return MakeError("the positional sounds are heard by the leader, actor " + std::to_string(Context.LeaderActor) + ", which has no script state");
			}
			return std::nullopt;
		}

		std::array<FHeardSound, FFieldActorSounds::ChannelCount> Heard{};
		const int32 ActorCount = static_cast<int32>(Context.States.size());
		for (int32 Actor = 0; Actor < ActorCount; ++Actor)
		{
			const FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
			if (State.SoundMode == FFieldActorSounds::Silent)
			{
				continue;
			}
			uint32 Squares = 0;
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Squares += static_cast<uint32>(GteSquare(Listener[Axis] - Whole(State.Position[Axis])));
			}
			if (static_cast<int32>(Squares) < 0)
			{
				return MakeError("actor " + std::to_string(Actor) + "'s positional sound lies so far from its listener that libgte's square root reads outside its table");
			}
			const int32 Distance = SquareRoot0(static_cast<int32>(Squares));
			// The farthest of the three kept so far makes room, the original's way of breaking ties.
			std::size_t Farthest = 0;
			if (Heard[0].Distance < Heard[1].Distance)
			{
				Farthest = Heard[1].Distance < Heard[2].Distance ? 2 : 1;
			}
			else
			{
				Farthest = Heard[0].Distance < Heard[2].Distance ? 2 : 0;
			}
			if (Distance < Heard[Farthest].Distance)
			{
				Heard[Farthest] = {Distance, State.Sound, State.SoundVolume, Actor, false};
			}
		}

		FFieldActorSounds& Sounds = Context.ActorSounds;
		std::array<bool, FFieldActorSounds::ChannelCount> bKept{};
		for (FHeardSound& Sound : Heard)
		{
			const int32 Channel = HeldChannel(Sounds, Sound);
			if (Channel != NoChannel)
			{
				bKept[static_cast<std::size_t>(Channel)] = true;
				Sound.bPlaying = true;
			}
		}
		for (int32 Channel = 0; Channel < FFieldActorSounds::ChannelCount; ++Channel)
		{
			FFieldActorSoundChannel& Held = Sounds.Channels[static_cast<std::size_t>(Channel)];
			if (!bKept[static_cast<std::size_t>(Channel)] && Held.Sound != FFieldActorSounds::Free)
			{
				if (Context.SoundEffects != nullptr)
				{
					Context.SoundEffects->StopEffectOn(DriverChannel(Channel));
				}
				Held = {};
			}
		}
		for (const FHeardSound& Sound : Heard)
		{
			if (Sound.Sound == NoSound)
			{
				continue;
			}
			if (Sound.bPlaying)
			{
				AdjustSound(Context, Sound);
			}
			else
			{
				StartSound(Context, Sound);
			}
		}
		return std::nullopt;
	}
}
