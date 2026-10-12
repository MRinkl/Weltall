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
	 * A cutscene director's script: the step it takes as each wait runs out, and the waits (ticks), up to the step
	 * that ends the scene.
	 */
	struct FWorldMapDirectorScript
	{
		std::vector<uint16> Steps;
		std::vector<uint16> Waits;
	};

	/** What scene 15 plays for one of its entrances. */
	struct FWorldMapShotEntrance
	{
		/** Where the scene starts (whole units). */
		std::array<int16, 3> Start{};
		/** Where the shot holds after each of its first flights (x, z). */
		std::array<int16, 2> Hold{};
		/** The sound effects of its last flight. */
		std::array<uint16, 3> Sounds{};
		FWorldMapDirectorScript Script;
		/** The field's entrance the scene leaves by. */
		uint16 FieldEntrance = 0;
	};

	/** The tables the directors of scenes 13, 15 and 16 play from, in the Worldmap program. */
	struct FWorldMapDirectorTables
	{
		static constexpr std::size_t ShotEntrances = 3;
		static constexpr std::size_t ShotRings = 5;

		FWorldMapDirectorScript Scene13;
		std::array<FWorldMapShotEntrance, ShotEntrances> Scene15;
		/** Scene 15's rings' widths (4.12), the first ring's first. */
		std::array<uint16, ShotRings> ShotRingScales{};
		FWorldMapDirectorScript Scene16;
	};

	/** Reads the directors' tables from the unpacked Worldmap program of a release. */
	WELTALLCORE_API TResult<FWorldMapDirectorTables> ReadWorldMapDirectorTables(std::span<const uint8> Program, EDiscRelease Release);
}
