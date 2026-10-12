#include "Input/PadInput.h"

#include <catch_amalgamated.hpp>

#include <vector>

using namespace Weltall;

namespace
{
	constexpr uint16 Up = FPadState::Bit(EPadButton::Up);
	constexpr uint16 Down = FPadState::Bit(EPadButton::Down);

	/** Repeats after 4 ticks held, then 2 ticks later, then every tick. */
	constexpr FPadRepeat MenuRepeat{4, 2, 4};

	/** The ticks on which a button repeats over a run of held states. */
	std::vector<int32> RepeatTicks(const std::vector<uint16>& Held, uint16 Button)
	{
		FPadReader Reader(MenuRepeat);
		std::vector<int32> Ticks;
		for (std::size_t Tick = 0; Tick < Held.size(); ++Tick)
		{
			if ((Reader.Next({Held[Tick]}).Repeated.Buttons & Button) != 0)
			{
				Ticks.push_back(static_cast<int32>(Tick));
			}
		}
		return Ticks;
	}
}

TEST_CASE("A held button repeats after its delay, then faster", "[Input]")
{
	// Pressed, then 5 ticks, 3 more, then every tick.
	CHECK(RepeatTicks(std::vector<uint16>(12, Up), Up) == std::vector<int32>{0, 5, 8, 9, 10, 11});

	// Letting go and pressing again starts over.
	std::vector<uint16> Tapped(6, Up);
	Tapped.push_back(0);
	Tapped.insert(Tapped.end(), 6, Up);
	CHECK(RepeatTicks(Tapped, Up) == std::vector<int32>{0, 5, 7, 12});
}

TEST_CASE("Pressing another button while one repeats puts the slower step back", "[Input]")
{
	// Up held from tick 0 repeats every tick from tick 8; down joins at tick 9: both repeat at
	// once, then the next repeat waits 3 ticks before every tick again.
	std::vector<uint16> Held(9, Up);
	Held.insert(Held.end(), 6, static_cast<uint16>(Up | Down));
	CHECK(RepeatTicks(Held, Up) == std::vector<int32>{0, 5, 8, 9, 12, 13, 14});
	CHECK(RepeatTicks(Held, Down) == std::vector<int32>{9, 12, 13, 14});
}
