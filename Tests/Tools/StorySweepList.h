#pragma once

#include "Foundation/Types.h"
#include "Support/PlayedGame.h"

#include <vector>

namespace Weltall::Testing
{
	/** A field map and the story's values its scenes are played at. */
	struct FStorySweepMap
	{
		int32 Map = 0;
		/**
		 * The values of the story's progress the map's scripts tell apart: each one they compare it with, and one of
		 * each run of values between two of those, before the first and after the last.
		 */
		std::vector<uint16> Stories;
		/**
		 * The scripts compare the progress with nothing: the values are those the nearest map that does compares it
		 * with, nearest by the scripts' own map changes. A map of scenes alone is entered at one of them in the story.
		 */
		bool bBorrowed = false;
	};

	/** Every field map of the first disc whose scripts can be read, with the story's values to play it at. */
	std::vector<FStorySweepMap> ListStorySweepMaps(const FPlayedGame& Game);
}
