#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	/**
	 * Runs a fixed-step simulation from the real time between display frames: it says how many
	 * ticks fall due as time passes and how far the display is between the last two, so a display
	 * of any rate shows smooth motion while every duration keeps its number of ticks.
	 */
	class FTickClock
	{
	public:
		/**
		 * After a stall (a hitch, a dragged window) no more than this many ticks' time runs at once and
		 * the rest of the time is dropped: the game waits instead of racing to catch up.
		 */
		static constexpr int32 MaxTicksPerAdvance = 4;

		/** A clock whose ticks are TickSeconds apart, e.g. a whole number of PsxTiming::FrameSeconds. */
		explicit FTickClock(double InTickSeconds) : TickSeconds(InTickSeconds) {}

		/**
		 * Lets Seconds of real time pass and returns how many ticks fell due. The simulation runs Speed
		 * times its own pace through them (a battle fast-forwarded): as many times the ticks, and the stall's limit.
		 */
		WELTALLCORE_API int32 Advance(double Seconds, double Speed = 1.0);

		/** How far the display is from the latest tick toward the next, 0 to 1. */
		float Alpha() const { return static_cast<float>(Elapsed / TickSeconds); }

	private:
		double TickSeconds;
		/** The time since the latest tick, always less than TickSeconds. */
		double Elapsed = 0.0;
	};
}
