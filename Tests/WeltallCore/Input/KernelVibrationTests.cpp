#include "Input/KernelVibration.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	/** How many of the next vertical blanks the port's motor is on for, up to Limit. */
	int32 VblanksOn(FKernelVibration& Vibration, int32 Port, int32 Limit)
	{
		int32 Count = 0;
		for (int32 Vblank = 0; Vblank < Limit; ++Vblank)
		{
			Vibration.Vblank();
			Count += Vibration.IsOn(Port) ? 1 : 0;
		}
		return Count;
	}
}

TEST_CASE("A port's motor runs for as many vertical blanks as it was started for, from the next one", "[Input]")
{
	FKernelVibration Vibration;
	Vibration.Vblank();
	CHECK_FALSE(Vibration.IsOn(0));
	Vibration.Start(0, 4);
	// Started between two vertical blanks: nothing is sent before the next.
	CHECK_FALSE(Vibration.IsOn(0));
	for (int32 Vblank = 0; Vblank < 4; ++Vblank)
	{
		Vibration.Vblank();
		CHECK(Vibration.IsOn(0));
		CHECK_FALSE(Vibration.IsOn(1));
	}
	Vibration.Vblank();
	CHECK_FALSE(Vibration.IsOn(0));
}

TEST_CASE("A new start replaces what a port's motor had left, and the ports run apart", "[Input]")
{
	FKernelVibration Vibration;
	Vibration.Start(0, 20);
	CHECK(VblanksOn(Vibration, 0, 5) == 5);
	// The shorter one takes the longer one's place: it is not added and the longer is not kept.
	Vibration.Start(0, 2);
	CHECK(VblanksOn(Vibration, 0, 10) == 2);

	Vibration.Start(1, 6);
	Vibration.Start(0, 3);
	for (int32 Vblank = 0; Vblank < 6; ++Vblank)
	{
		Vibration.Vblank();
		CHECK(Vibration.IsOn(1));
		CHECK(Vibration.IsOn(0) == (Vblank < 3));
	}
	Vibration.Vblank();
	CHECK_FALSE(Vibration.IsOn(1));
}
