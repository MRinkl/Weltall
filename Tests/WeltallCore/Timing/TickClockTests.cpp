#include "Timing/PsxTiming.h"
#include "Timing/TickClock.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	/** A tick of two frames, a rate PlayStation games commonly run their logic at. */
	constexpr double TwoFrames = 2.0 * PsxTiming::FrameSeconds;
}

TEST_CASE("A frame of the PlayStation's NTSC picture lasts 263 lines of 3413 GPU clocks", "[Timing]")
{
	CHECK(PsxTiming::FrameSeconds == Catch::Approx(1.0 / 59.81733).epsilon(1e-7));
	CHECK(TwoFrames == Catch::Approx(1795238.0 / 53693175.0).epsilon(1e-12));
}

TEST_CASE("Ticks fall due at the game's rate whatever the display's rate", "[Timing]")
{
	for (const int32 DisplayHz : {30, 60, 75, 120, 144, 165, 240})
	{
		FTickClock Clock(TwoFrames);
		int64 Ticks = 0;
		// An hour of display frames: the count drifts by no tick at all.
		for (int32 Frame = 0; Frame < 3600 * DisplayHz; ++Frame)
		{
			Ticks += Clock.Advance(1.0 / DisplayHz);
		}
		INFO(DisplayHz << " Hz");
		CHECK(Ticks == 107671);
	}
}

// Durations below are exact in binary, so no tick boundary is left to rounding.
TEST_CASE("The display shows how far it is between the last two ticks", "[Timing]")
{
	FTickClock Clock(0.125);
	CHECK(Clock.Advance(0.03125) == 0);
	CHECK(Clock.Alpha() == 0.25f);
	CHECK(Clock.Advance(0.125) == 1);
	CHECK(Clock.Alpha() == 0.25f);
	CHECK(Clock.Advance(0.09375) == 1);
	CHECK(Clock.Alpha() == 0.0f);
	CHECK(Clock.Advance(0.3125) == 2);
	CHECK(Clock.Alpha() == 0.5f);
}

TEST_CASE("Frames as long as a tick run one tick each", "[Timing]")
{
	FTickClock Clock(TwoFrames);
	CHECK(Clock.Advance(TwoFrames / 3.0) == 0);
	for (int32 Frame = 0; Frame < 100000; ++Frame)
	{
		REQUIRE(Clock.Advance(TwoFrames) == 1);
		REQUIRE(Clock.Alpha() == Catch::Approx(1.0 / 3.0).epsilon(1e-6));
	}
}

TEST_CASE("After a stall the clock runs a few ticks and drops the rest", "[Timing]")
{
	FTickClock Clock(0.125);
	CHECK(Clock.Advance(1.0625) == FTickClock::MaxTicksPerAdvance);
	CHECK(Clock.Alpha() == 0.5f);
	CHECK(Clock.Advance(0.0625) == 1);
	CHECK(Clock.Alpha() == 0.0f);
}

TEST_CASE("A clock run faster counts as many times the ticks, and lets a stall run as many times more", "[Timing]")
{
	for (const double Speed : {1.5, 2.0, 3.0, 4.0, 6.0, 8.0})
	{
		FTickClock Clock(TwoFrames);
		int64 Ticks = 0;
		// Ten minutes of a 60 Hz display.
		for (int32 Frame = 0; Frame < 600 * 60; ++Frame)
		{
			Ticks += Clock.Advance(1.0 / 60.0, Speed);
		}
		INFO(Speed << " times");
		CHECK(Ticks == static_cast<int64>(600.0 * Speed / TwoFrames));
	}

	FTickClock Clock(0.125);
	CHECK(Clock.Advance(0.0625, 4.0) == 2);
	CHECK(Clock.Alpha() == 0.0f);
	CHECK(Clock.Advance(0.046875, 4.0) == 1);
	CHECK(Clock.Alpha() == 0.5f);
	// A stall: four ticks' time at the game's pace, which is sixteen ticks at four times.
	CHECK(Clock.Advance(10.0, 4.0) == 4 * FTickClock::MaxTicksPerAdvance);
	CHECK(Clock.Advance(10.0, 1.5) == 6);
	CHECK(Clock.Advance(0.125, 0.0) == 0);
}

TEST_CASE("No time or a clock running backwards runs nothing", "[Timing]")
{
	FTickClock Clock(0.125);
	CHECK(Clock.Advance(0.0) == 0);
	CHECK(Clock.Advance(-1.0) == 0);
	CHECK(Clock.Alpha() == 0.0f);
}
