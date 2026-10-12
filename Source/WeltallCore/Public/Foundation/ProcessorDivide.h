#pragma once

#include "Foundation/Types.h"

#include <limits>

namespace Weltall
{
	/**
	 * The PlayStation processor's signed division where the compiler left out its checks: a zero
	 * divisor gives -1 (or 1 for a negative dividend), and the one overflow wraps.
	 */
	constexpr int32 ProcessorDivide(int32 Dividend, int32 Divisor)
	{
		if (Divisor == 0)
		{
			return Dividend >= 0 ? -1 : 1;
		}
		if (Dividend == std::numeric_limits<int32>::min() && Divisor == -1)
		{
			return Dividend;
		}
		return Dividend / Divisor;
	}

	/** The signed remainder beside it: a zero divisor leaves the dividend, and the overflow leaves 0. */
	constexpr int32 ProcessorRemainder(int32 Dividend, int32 Divisor)
	{
		if (Divisor == 0)
		{
			return Dividend;
		}
		if (Dividend == std::numeric_limits<int32>::min() && Divisor == -1)
		{
			return 0;
		}
		return Dividend % Divisor;
	}

	/** The unsigned division: a zero divisor gives all ones. */
	constexpr uint32 ProcessorDivideUnsigned(uint32 Dividend, uint32 Divisor)
	{
		return Divisor == 0 ? 0xFFFFFFFFu : Dividend / Divisor;
	}

	/** The unsigned remainder: a zero divisor leaves the dividend. */
	constexpr uint32 ProcessorRemainderUnsigned(uint32 Dividend, uint32 Divisor)
	{
		return Divisor == 0 ? Dividend : Dividend % Divisor;
	}
}
