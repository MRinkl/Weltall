#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	/**
	 * Where the sound driver's timer interrupts fall among the SPU's samples. Both run off the
	 * console's system clock: the SPU makes a sample every 768 cycles, 96 counts of the timer (which
	 * counts the system clock / 8), and the timer interrupts every 17,641 counts (it counts up to its
	 * target, 44E8h, and restarts at 0 on the next count), so interrupts come 183 73/96 samples apart,
	 * the first 17,641 counts after the driver starts. An interrupt between two sample times runs
	 * before the later sample: its writes reach the SPU with the first sample made after it.
	 */
	class FGameSoundClock
	{
	public:
		static constexpr int32 CountsPerSample = 96;
		static constexpr int32 CountsPerInterrupt = 17641;

		/** Whether an interrupt falls before the next sample. */
		bool IsInterruptDue() const { return CountsToInterrupt <= 0; }

		/** The samples made before the next interrupt falls due. */
		int32 SamplesToInterrupt() const { return CountsToInterrupt <= 0 ? 0 : (CountsToInterrupt + CountsPerSample - 1) / CountsPerSample; }

		/** The due interrupt has run. */
		void RunInterrupt() { CountsToInterrupt += CountsPerInterrupt; }

		/** Samples have been made, no more than SamplesToInterrupt(). */
		void MakeSamples(int32 Samples) { CountsToInterrupt -= Samples * CountsPerSample; }

	private:
		/** From the next sample's time to the next interrupt's, in timer counts. */
		int32 CountsToInterrupt = CountsPerInterrupt;
	};
}
