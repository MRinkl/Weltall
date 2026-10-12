#pragma once

#include "Graphics/GteMatrix.h"

#include <array>
#include <cstddef>

namespace Weltall::GteArithmetic
{
	/** An intermediate MAC1-3 value: flags an overflow of the GTE's 44-bit accumulator and wraps. */
	inline int64 Accumulate(std::size_t Index, int64 Value, uint32& Flag)
	{
		constexpr int64 MacMaximum = 0x7FFFFFFFFFF;
		constexpr int64 MacMinimum = -0x80000000000;
		constexpr std::array<uint32, 3> PositiveOverflow = {1u << 30, 1u << 29, 1u << 28};
		constexpr std::array<uint32, 3> NegativeOverflow = {1u << 27, 1u << 26, 1u << 25};
		if (Value > MacMaximum)
		{
			Flag |= PositiveOverflow[Index];
		}
		else if (Value < MacMinimum)
		{
			Flag |= NegativeOverflow[Index];
		}
		return static_cast<int64>(static_cast<uint64>(Value) << 20) >> 20;
	}

	/** One MVMVA row: the 44-bit sum Start + Matrix row * Vector. */
	inline int64 RowTimes(const FGteMatrix& Matrix, std::size_t Row, const std::array<int32, 3>& Vector, uint32& Flag, int64 Start = 0)
	{
		int64 Sum = Accumulate(Row, Start, Flag);
		for (std::size_t Column = 0; Column < 3; ++Column)
		{
			Sum = Accumulate(Row, Sum + int64{Matrix.M[Row][Column]} * Vector[Column], Flag);
		}
		return Sum;
	}

	/** A 44-bit sum shifted by sf = 1 into its 32-bit MAC register. */
	inline int32 Mac(int64 Sum)
	{
		return static_cast<int32>(Sum >> 12);
	}

	/** The flag IR1-3 raise when a MAC value does not fit them under lm = 0. */
	inline uint32 IrSaturation(std::size_t Index, int32 Value)
	{
		constexpr std::array<uint32, 3> Bits = {1u << 24, 1u << 23, 1u << 22};
		return Value < -0x8000 || Value > 0x7FFF ? Bits[Index] : 0;
	}

	/** The flag register with its error bit: set when any of bits 30-23 or 18-13 is. */
	inline uint32 WithErrorBit(uint32 Flag)
	{
		return (Flag & 0x7F87E000u) != 0 ? Flag | 0x80000000u : Flag;
	}
}
