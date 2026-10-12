#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <array>

namespace Weltall
{
	class FPsxExecutable;

	/** libgte's arctangent table as the executable holds it: the angle (4096 a turn) of tan = n / 1024. */
	struct FGteArctangents
	{
		std::array<int16, 1025> Angles{};

		WELTALLCORE_API static TResult<FGteArctangents> Read(const FPsxExecutable& Executable);
	};

	/** libgte's ratan2: the angle of (X, Y), 4096 a turn, from -2048 to 2048; 0 for (0, 0). */
	WELTALLCORE_API int32 GteRatan2(int32 Y, int32 X, const FGteArctangents& Table);

	/** What the GTE's SQR leaves for one component: the square of its low 16 bits, signed. */
	inline int32 GteSquare(int32 Value)
	{
		const int32 Low = static_cast<int16>(Value);
		return Low * Low;
	}
}
