#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	/** A song's full volume, which the world map plays and resumes its songs at. */
	inline constexpr int32 WorldMapFullVolume = 0x7F;
	/** The driver's ticks the world map's song fades in and out over. */
	inline constexpr int32 WorldMapSongFadeTicks = 0xF0;

	/** The world map's songs, files of its scene group. */
	enum class EWorldMapSong : uint8
	{
		/** The map's (the group's header + 5). */
		Map,
		/** The Yggdrasil's while it flies (+ 7). */
		Vehicle,
		/** The battle's, which the world map starts as it leaves for one (+ 8). */
		Battle,
	};

	/**
	 * The sound driver as the world map calls it, each call where the original makes it. The world map's
	 * song is the driver's current sequence, kept aside across a battle; the calls name the scene group's
	 * files by what they are. The world map is silent without one.
	 */
	class IWorldMapSound
	{
	public:
		virtual ~IWorldMapSound() = default;

		/** The scene group the calls' files are read from, by its header file (directory 36). */
		virtual void SetSceneGroup(int32 Header) = 0;
		/** The field's song stopped and its bank unloaded. */
		virtual void StopFieldSong() = 0;
		/** The scene's instruments (header + 4). */
		virtual void LoadBank() = 0;
		/** The scene's effects registered and unregistered (header + 6). */
		virtual void RegisterEffects() = 0;
		virtual void UnregisterEffects() = 0;
		/** Song loaded as the current sequence; the one before is left as it was. */
		virtual void LoadSong(EWorldMapSong Song) = 0;
		/** The current sequence played, resumed or its volume set: Volume over Ticks of the driver's. */
		virtual void PlaySong(int32 Volume, int32 Ticks) = 0;
		virtual void ResumeSong(int32 Volume, int32 Ticks) = 0;
		virtual void SetSongVolume(int32 Volume, int32 Ticks) = 0;
		/** The current sequence freed. */
		virtual void FreeSong() = 0;
		/** Every sequence stopped. */
		virtual void StopAllSongs() = 0;
		/** The current sequence kept for the return from a battle, and the kept one made current again. */
		virtual void KeepSong() = 0;
		virtual void TakeKeptSong() = 0;
		/** The scene's effect Effect played; every effect stopped. */
		virtual void PlayEffect(int32 Effect) = 0;
		/** The scene's effect Effect on the automatic effects' first channels, protected from them. */
		virtual void PlayProtectedEffect(int32 Effect) = 0;
		virtual void StopAllEffects() = 0;
		/** The scene's effect Effect's volume as it plays: Volume at once, or over Ticks of the driver's. */
		virtual void SetEffectVolume(int32 Effect, int32 Volume) = 0;
		virtual void FadeEffectVolume(int32 Effect, int32 Volume, int32 Ticks) = 0;
		/** The sound paused and resumed. */
		virtual void Pause() = 0;
		virtual void Resume() = 0;
	};
}
