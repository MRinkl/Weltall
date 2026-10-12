#include "Sound/XaResampler.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

TEST_CASE("The XA resampler makes seven stereo frames of every six input frames", "[Sound]")
{
	FXaResampler Resampler;
	std::vector<int16> Out;
	Resampler.Resample(std::vector<int16>(5 * 2), 2, Out);
	CHECK(Out.empty());
	Resampler.Resample(std::vector<int16>(7 * 2), 2, Out);
	CHECK(Out.size() == 14 * 2);
	Resampler.Resample(std::vector<int16>(6), 1, Out);
	CHECK(Out.size() == 21 * 2);
}

TEST_CASE("The XA resampler weighs its latest inputs with the drive's zigzag filters", "[Sound]")
{
	// An impulse five inputs before the sixth meets each phase's sixth weight: -2, 3, -5, 5, 10, 1Ah, -44h
	// (in 32768ths, floored).
	FXaResampler Resampler;
	std::vector<int16> Out;
	Resampler.Resample(std::vector<int16>{32767, 0, 0, 0, 0, 0}, 1, Out);
	REQUIRE(Out.size() == 7 * 2);
	const int16 Expected[] = {-2, 2, -5, 4, 9, 25, -68};
	for (std::size_t Phase = 0; Phase < 7; ++Phase)
	{
		CHECK(Out[Phase * 2] == Expected[Phase]);
		CHECK(Out[Phase * 2 + 1] == Expected[Phase]);
	}
}
