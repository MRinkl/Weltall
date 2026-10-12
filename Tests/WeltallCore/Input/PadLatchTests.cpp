#include "Input/PadLatch.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	FPadState Holding(std::initializer_list<EPadButton> Buttons)
	{
		FPadState Pad;
		for (const EPadButton Button : Buttons)
		{
			Pad.Buttons |= FPadState::Bit(Button);
		}
		return Pad;
	}
}

TEST_CASE("A press shorter than a tick reaches the next tick once", "[Input]")
{
	FPadLatch Latch;
	Latch.Sample(Holding({EPadButton::Circle}));
	Latch.Sample(Holding({}));
	CHECK(Latch.TakeForTick() == Holding({EPadButton::Circle}));
	CHECK(Latch.TakeForTick() == Holding({}));
}

TEST_CASE("A held button stays held and a released one is gone at the next tick", "[Input]")
{
	FPadLatch Latch;
	Latch.Sample(Holding({EPadButton::Up, EPadButton::Square}));
	CHECK(Latch.TakeForTick() == Holding({EPadButton::Up, EPadButton::Square}));
	Latch.Sample(Holding({EPadButton::Up, EPadButton::Square}));
	Latch.Sample(Holding({EPadButton::Up}));
	CHECK(Latch.TakeForTick() == Holding({EPadButton::Up}));
	CHECK(Latch.TakeForTick() == Holding({EPadButton::Up}));
}

TEST_CASE("A button pressed again between ticks is pressed again", "[Input]")
{
	FPadLatch Latch;
	Latch.Sample(Holding({EPadButton::Circle}));
	CHECK(Latch.TakeForTick() == Holding({EPadButton::Circle}));
	Latch.Sample(Holding({}));
	Latch.Sample(Holding({EPadButton::Circle}));
	Latch.Sample(Holding({}));
	CHECK(Latch.TakeForTick() == Holding({EPadButton::Circle}));
	CHECK(Latch.TakeForTick() == Holding({}));
}

TEST_CASE("Several ticks in one frame see a short press only once", "[Input]")
{
	FPadLatch Latch;
	Latch.Sample(Holding({EPadButton::Cross, EPadButton::Left}));
	Latch.Sample(Holding({EPadButton::Left}));
	CHECK(Latch.TakeForTick() == Holding({EPadButton::Cross, EPadButton::Left}));
	CHECK(Latch.TakeForTick() == Holding({EPadButton::Left}));
	CHECK(Latch.TakeForTick() == Holding({EPadButton::Left}));
}
