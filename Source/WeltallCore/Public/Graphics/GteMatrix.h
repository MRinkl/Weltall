#pragma once

#include "Foundation/Types.h"

#include <array>

namespace Weltall
{
	/**
	 * The PlayStation's MATRIX: a 3 x 3 rotation and scale in 4.12 fixed point (4096 = 1.0) and a
	 * translation. Applied to a column vector: v' = M v + T.
	 */
	struct FGteMatrix
	{
		static constexpr int32 One = 4096;

		std::array<std::array<int16, 3>, 3> M{};
		std::array<int32, 3> T{};

		static constexpr FGteMatrix Identity()
		{
			FGteMatrix Result;
			Result.M[0][0] = Result.M[1][1] = Result.M[2][2] = One;
			return Result;
		}

		bool operator==(const FGteMatrix&) const = default;
	};

	/**
	 * libgte's ScaleMatrix: multiplies column j by Scale[j] (4.12) and keeps only the low 16 bits of
	 * each product, so a scale that overflows an entry wraps around instead of saturating.
	 */
	WELTALLCORE_API FGteMatrix ScaleColumns(const FGteMatrix& Matrix, const std::array<int32, 3>& Scale);
}
