#include "Sound/GameSoundClock.h"
#include "Support/XorShift32.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <vector>

using namespace Weltall;

namespace
{
	/** Interrupt k (from 1) falls 17,641k timer counts after the start, before the first sample at or after it (96 counts each). */
	int64 SampleOfInterrupt(int64 Interrupt)
	{
		return (Interrupt * FGameSoundClock::CountsPerInterrupt + FGameSoundClock::CountsPerSample - 1) / FGameSoundClock::CountsPerSample;
	}
}

TEST_CASE("The sound driver's interrupts fall before the samples their timer counts reach", "[Sound]")
{
	// Samples made in uneven buffers, as an audio device asks for them: every interrupt runs just
	// before its sample, whatever the buffers.
	FGameSoundClock Clock;
	Testing::FXorShift32 Random(0xC10C);
	int64 Made = 0;
	int64 Interrupts = 0;
	int64 Misplaced = 0;
	int64 Miscounted = 0;
	constexpr int64 Hour = 44100LL * 3600;
	while (Made < Hour)
	{
		int64 Buffer = std::min<int64>(1 + Random.Next() % 4096, Hour - Made);
		while (Buffer > 0)
		{
			if (Clock.IsInterruptDue())
			{
				++Interrupts;
				Misplaced += Made != SampleOfInterrupt(Interrupts) ? 1 : 0;
				Clock.RunInterrupt();
			}
			const int64 Count = std::min<int64>(Buffer, Clock.SamplesToInterrupt());
			Clock.MakeSamples(static_cast<int32>(Count));
			Made += Count;
			Buffer -= Count;
		}
		// The interrupts run so far are exactly those that fall before the last sample made.
		Miscounted += Interrupts != (Made - 1) * FGameSoundClock::CountsPerSample / FGameSoundClock::CountsPerInterrupt ? 1 : 0;
	}
	CHECK(Misplaced == 0);
	CHECK(Miscounted == 0);
	// 239.986 a second: an hour holds 863,951 of them, not 864,000.
	CHECK(Interrupts == 863951);
}

TEST_CASE("The sound driver's interrupts come 183 or 184 samples apart, the first after 184", "[Sound]")
{
	FGameSoundClock Clock;
	CHECK_FALSE(Clock.IsInterruptDue());
	CHECK(Clock.SamplesToInterrupt() == 184);
	std::vector<int32> Gaps;
	for (int32 Interrupt = 0; Interrupt < 96; ++Interrupt)
	{
		Gaps.push_back(Clock.SamplesToInterrupt());
		Clock.MakeSamples(Clock.SamplesToInterrupt());
		REQUIRE(Clock.IsInterruptDue());
		CHECK(Clock.SamplesToInterrupt() == 0);
		Clock.RunInterrupt();
	}
	// 96 interrupts are 17,641 x 96 counts: exactly 17,641 samples.
	int32 Total = 0;
	for (const int32 Gap : Gaps)
	{
		CHECK((Gap == 183 || Gap == 184));
		Total += Gap;
	}
	CHECK(Total == 17641);
}
