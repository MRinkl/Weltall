#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <array>
#include <optional>

namespace Weltall
{
	struct FEventContext;

	/** Who hears the actors' positional sounds (FE 8A). */
	namespace FieldSoundListener
	{
		constexpr uint16 Leader = 0;
		constexpr uint16 CameraEye = 1;
		constexpr uint16 CameraTarget = 2;
	}

	/** A channel the positional sounds play on: the actor holding it and its sound, both Free when none does. */
	struct FFieldActorSoundChannel
	{
		uint16 Owner = 0xFFFF;
		uint16 Sound = 0xFFFF;
	};

	/**
	 * The sounds actors give off where they stand (FE 13, FE 14): each tick the three sounding actors
	 * nearest the listener play on the sound driver's effect channels 0, 2 and 4 (the scripts' channels
	 * 0 to 2), louder the nearer, panned by where they stand on screen. A map's load resets it.
	 */
	struct FFieldActorSounds
	{
		static constexpr int32 ChannelCount = 3;
		static constexpr uint16 Free = 0xFFFF;
		/** An actor's SoundMode while it gives off no sound. */
		static constexpr uint8 Silent = 0xFF;

		std::array<FFieldActorSoundChannel, ChannelCount> Channels{};
		/** FieldSoundListener; any other value leaves the sounds as they are. */
		uint16 Listener = FieldSoundListener::Leader;
		/** How far a sound carries: it fades linearly to a 127th of its volume there (FE 98). Read signed. */
		int16 Range = 0x3FF;
		/** The scripts' channels (bit n: channel n) a map change is to leave playing (FE 8D). */
		uint16 KeepMask = 0;
	};

	/**
	 * A map change or the field's exit: every channel is free again, and of the driver's effect
	 * channels 0, 2, 4 and 6 (the scripts' 0 to 3) those FE 8D did not ask to keep stop.
	 */
	WELTALLCORE_API void StopFieldActorSoundsForExit(FEventContext& Context);

	/** FE 13 and FE 14: the channel the actor holds, if it holds one, stops and is free again. */
	WELTALLCORE_API void ReleaseActorSound(FEventContext& Context, int32 Actor);

	/**
	 * One tick of the positional sounds, as the field runs them after the camera's update: the three
	 * sounding actors nearest the listener keep or take a channel and the other channels stop; each
	 * sound's volume follows its distance and its pan the actor's place across the screen (through the
	 * view the camera built and the projection distance the GTE holds, the scripts' own). An error when
	 * the listener is the leader and it has no script state, or a distance's squares pass 31 bits,
	 * where libgte's square root reads outside its table.
	 */
	[[nodiscard]] WELTALLCORE_API std::optional<FError> UpdateFieldActorSounds(FEventContext& Context);
}
