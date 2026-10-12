#pragma once

#include "Foundation/Types.h"

#include <limits>

namespace Weltall::SoundInteger
{
	/** The driver's 32-bit arithmetic, which wraps as the PlayStation's CPU does. */
	inline int32 Add(int32 Left, int32 Right)
	{
		return static_cast<int32>(static_cast<uint32>(Left) + static_cast<uint32>(Right));
	}

	inline int32 Subtract(int32 Left, int32 Right)
	{
		return static_cast<int32>(static_cast<uint32>(Left) - static_cast<uint32>(Right));
	}

	inline int32 Negate(int32 Value)
	{
		return static_cast<int32>(0u - static_cast<uint32>(Value));
	}

	inline int32 ShiftLeft(int32 Value, int32 Bits)
	{
		return static_cast<int32>(static_cast<uint32>(Value) << Bits);
	}

	/** The high half of a 16.16 or 8.24 value, signed. */
	inline int16 High(int32 Value)
	{
		return static_cast<int16>(static_cast<uint32>(Value) >> 16);
	}

	/** Division truncating toward zero as the CPU's; every caller checks the divisor first. */
	inline int32 Divide(int32 Dividend, int32 Divisor)
	{
		if (Divisor == 0)
		{
			return 0;
		}
		if (Dividend == std::numeric_limits<int32>::min() && Divisor == -1)
		{
			return Dividend;
		}
		return Dividend / Divisor;
	}
}
