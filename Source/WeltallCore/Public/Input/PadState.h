#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	/** The buttons of a PlayStation controller, numbered by their bit in the pad's report. */
	enum class EPadButton : uint8
	{
		Select = 0,
		L3 = 1,
		R3 = 2,
		Start = 3,
		Up = 4,
		Right = 5,
		Down = 6,
		Left = 7,
		L2 = 8,
		R2 = 9,
		L1 = 10,
		R1 = 11,
		Triangle = 12,
		Circle = 13,
		Cross = 14,
		Square = 15,
	};

	/**
	 * The buttons held on one tick. The game reads the original controller, so the port's input
	 * layer turns whatever the player uses into this.
	 */
	struct FPadState
	{
		uint16 Buttons = 0;

		static constexpr uint16 Bit(EPadButton Button) { return static_cast<uint16>(1u << static_cast<uint32>(Button)); }

		constexpr bool IsHeld(EPadButton Button) const { return (Buttons & Bit(Button)) != 0; }

		bool operator==(const FPadState&) const = default;
	};
}
