#pragma once

#include "Foundation/Types.h"

namespace Weltall::Testing
{
	/**
	 * Marsaglia's 32-bit xorshift. Tests that compare against a research script draw their cases
	 * from it, so both sides generate the same cases from the same seed.
	 */
	class FXorShift32
	{
	public:
		explicit FXorShift32(uint32 Seed) : State(Seed) {}

		uint32 Next()
		{
			State ^= State << 13;
			State ^= State >> 17;
			State ^= State << 5;
			return State;
		}

		/**
		 * The next value modulo Limit. It lives out of line: MSVC's code generator does not finish a function
		 * that takes a value modulo a limit drawn a line before (known to lie in a small range) when it can see both.
		 */
		uint32 Below(uint32 Limit);

	private:
		uint32 State;
	};
}
