#pragma once

#include "Foundation/Types.h"

#include <array>

namespace Weltall::BattleHudDigits
{
	/** A place that draws no digit. */
	inline constexpr uint8 Blank = 0xFF;

	/** The battle's digits of a number: nine decimal digits (the first may pass 9), leading zeros blank, a lone 0 kept. */
	inline std::array<uint8, 9> ToDigits(uint32 Value)
	{
		std::array<uint8, 9> Digits{};
		uint32 Power = 100000000;
		for (uint8& Digit : Digits)
		{
			Digit = static_cast<uint8>(Value / Power);
			Value %= Power;
			Power /= 10;
		}
		for (std::size_t Index = 1; Index < Digits.size(); ++Index)
		{
			if (Digits[Index] != 0)
			{
				if (Digits[Index - 1] == 0)
				{
					Digits[Index - 1] = Blank;
				}
				return Digits;
			}
			Digits[Index - 1] = Blank;
		}
		return Digits;
	}
}
