#pragma once

#include "Foundation/Types.h"

// The processor's 32-bit arithmetic, which the field's movement leaves unchecked.
namespace Weltall::MovementRules
{
	/** A value as a 32-bit register holds it: the low 32 bits, two's complement. */
	inline int32 Wrap(int64 Value)
	{
		return static_cast<int32>(static_cast<uint32>(static_cast<uint64>(Value)));
	}

	inline int32 Multiply(int64 A, int64 B)
	{
		return Wrap(A * B);
	}

	inline int32 Add(int64 A, int64 B)
	{
		return Wrap(A + B);
	}

	/** The whole part of a 16.16 value, as the field reads it: its upper 16 bits, signed. */
	inline int32 WholePart(int32 Value)
	{
		return static_cast<int16>(static_cast<uint32>(Value) >> 16);
	}
}
