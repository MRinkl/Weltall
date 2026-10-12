#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	/**
	 * The console's play clock: it counts every vertical blank from power-on, 60 to a second, through
	 * pauses, menus and loading alike, and stops for good at 100 hours. Only the field's scripts read it
	 * (as minutes and seconds, and hours); the play time the menus show and saves keep is the game
	 * state's, which pauses do not count.
	 */
	struct FPlayClock
	{
		static constexpr uint8 StopHours = 100;

		uint8 Frames = 0;
		uint8 Seconds = 0;
		uint8 Minutes = 0;
		uint8 Hours = 0;
		bool bStopped = false;

		/** One vertical blank. */
		WELTALLCORE_API void Vblank();
	};
}
