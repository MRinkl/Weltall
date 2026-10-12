#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Media/DiscIdentification.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	/**
	 * A cutscene's script as the program holds it: instructions of two or four halfwords (the operation, then its
	 * arguments), up to and with the one that ends the scene.
	 */
	using FWorldMapScript = std::vector<int16>;

	/** The operations of a cutscene's script, and the halfwords each instruction takes. */
	namespace WorldMapScriptOp
	{
		inline constexpr int32 End = 0;
		inline constexpr int32 Wait = 1;
		inline constexpr int32 SendMessage = 2;
		inline constexpr int32 SetStart = 3;
		inline constexpr int32 SetPlace = 4;
		inline constexpr int32 StartEffect = 5;
		inline constexpr int32 StopEffect = 6;
		inline constexpr int32 StopParticles = 7;
		inline constexpr int32 SongVolume = 8;
		inline constexpr int32 PlaySound = 9;
		inline constexpr int32 FadeSound = 10;
		inline constexpr int32 SetFade = 11;
		inline constexpr int32 Count = 12;

		/** Halfwords of an instruction: 2 for the short ones, 4 for those with three arguments. */
		WELTALLCORE_API int32 Halfwords(int32 Op);
	}

	/**
	 * Where scene 17's five ribbons go at one of their stops, a record a ribbon: its place (x, y, z), its colour
	 * (red, green, blue) and the steps of its red and green, a word nothing reads, its turn about y, its wave's phase,
	 * its length and the length's growth a tick, and another word nothing reads.
	 */
	using FWorldMapRibbonStop = std::array<std::array<int16, 14>, 5>;

	/** The tables of the cutscenes a script runs (scenes 17 and 18). */
	struct FWorldMapScriptTables
	{
		FWorldMapScript Scene17;
		FWorldMapScript Scene18;
		std::array<FWorldMapRibbonStop, 3> RibbonStops{};
	};

	/** Reads them from the unpacked Worldmap program of a release. */
	WELTALLCORE_API TResult<FWorldMapScriptTables> ReadWorldMapScriptTables(std::span<const uint8> Program, EDiscRelease Release);
}
