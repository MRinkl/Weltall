#pragma once

#include "Input/PadState.h"

namespace Weltall
{
	/**
	 * How a game's pad code repeats held buttons for its menus: a newly pressed button repeats at
	 * once. While buttons stay held a count runs, and when it has passed Delay the held buttons
	 * repeat and the count starts again from the reload: FirstReload after any change of what is
	 * held, NextReload after that.
	 */
	struct FPadRepeat
	{
		uint8 Delay = 0;
		uint8 FirstReload = 0;
		uint8 NextReload = 0;
	};

	/** The controller as a game reads it once per tick. */
	struct FPadInput
	{
		FPadState Held;
		/** Held, and not held on the tick before. */
		FPadState Pressed;
		/** Newly pressed, or held and repeating. */
		FPadState Repeated;

		bool operator==(const FPadInput&) const = default;
	};

	/** Reads the controller tick by tick, remembering what the tick before held. */
	class FPadReader
	{
	public:
		explicit FPadReader(FPadRepeat InRepeat) : Repeat(InRepeat) {}

		FPadInput Next(FPadState Held)
		{
			FPadInput Input{Held, {static_cast<uint16>(Held.Buttons & ~Previous.Buttons)}, {}};
			if (Held.Buttons != Previous.Buttons)
			{
				Reload = Repeat.FirstReload;
			}
			if ((Held.Buttons & Previous.Buttons) == 0)
			{
				Count = 0;
				Reload = Repeat.FirstReload;
				Input.Repeated = Input.Pressed;
			}
			else if (Count < Repeat.Delay)
			{
				++Count;
			}
			else
			{
				Count = Reload;
				Reload = Repeat.NextReload;
				Input.Repeated = Held;
			}
			Previous = Held;
			return Input;
		}

	private:
		FPadRepeat Repeat;
		FPadState Previous;
		uint8 Count = 0;
		uint8 Reload = 0;
	};
}
