#pragma once

#include "Foundation/Types.h"

#include <array>

namespace Weltall
{
	/**
	 * The controllers' motors as the kernel runs them, one for each port: a program starts a port's motor for a
	 * number of vertical blanks, and at every vertical blank the kernel sends the port's controller the motor's
	 * state, on while any are left. The original's bytes run one motor with no levels (a DualShock's small one),
	 * so a vibration is only as long as it lasts.
	 */
	class FKernelVibration
	{
	public:
		static constexpr int32 Ports = 2;

		/** The port's motor runs for the next Vblanks vertical blanks, in place of whatever it had left. */
		WELTALLCORE_API void Start(int32 Port, int32 Vblanks);

		/** One vertical blank: each port's motor is on while it has vertical blanks left, which this one counts. */
		WELTALLCORE_API void Vblank();

		/** Whether the last vertical blank sent the port's controller its motor on. */
		bool IsOn(int32 Port) const { return On[static_cast<std::size_t>(Port)]; }

	private:
		std::array<uint16, Ports> Left{};
		std::array<bool, Ports> On{};
	};
}
