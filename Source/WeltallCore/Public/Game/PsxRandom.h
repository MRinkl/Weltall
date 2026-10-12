#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	/**
	 * The PlayStation C library's rand(): a linear congruential generator on a 32-bit seed that
	 * returns bits 16 to 30 of each new seed; srand() sets the seed.
	 */
	class FPsxRandom
	{
	public:
		explicit FPsxRandom(uint32 InSeed = 0)
			: State(InSeed)
		{
		}

		/** 0 to 0x7FFF. */
		int32 Next()
		{
			State = State * 0x41C64E6Du + 0x3039u;
			return static_cast<int32>((State >> 16) & 0x7FFF);
		}

		/** The low byte of the next number. */
		uint8 NextByte() { return static_cast<uint8>(Next()); }

		uint32 Seed() const { return State; }

	private:
		uint32 State;
	};

	/**
	 * The kernel's byte from Low to High: FFh when Low is, 0 when High is, Low when they are equal
	 * (none of these draws), else Low plus the next number's low byte modulo the span.
	 */
	WELTALLCORE_API uint8 RandomByteBetween(FPsxRandom& Random, uint8 Low, uint8 High);
}
