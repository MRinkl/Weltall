#pragma once

#include "Input/PadState.h"

namespace Weltall
{
	/**
	 * The buttons each tick sees when the controller is read at every display frame, more often
	 * than the game ticks: those held at the latest frame, and any pressed since the tick before.
	 * A press shorter than a tick still reaches the game, and a release is seen as soon as the
	 * game would see it.
	 */
	class FPadLatch
	{
	public:
		/** Records the buttons held at a display frame. */
		void Sample(FPadState Held)
		{
			Pressed.Buttons |= Held.Buttons & ~Latest.Buttons;
			Latest = Held;
		}

		/** The buttons for the next tick. */
		FPadState TakeForTick()
		{
			const FPadState Taken{static_cast<uint16>(Latest.Buttons | Pressed.Buttons)};
			Pressed = {};
			return Taken;
		}

	private:
		FPadState Latest;
		FPadState Pressed;
	};
}
