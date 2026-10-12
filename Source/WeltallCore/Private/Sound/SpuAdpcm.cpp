#include "Sound/SpuAdpcm.h"

#include <algorithm>
#include <array>

namespace Weltall
{
	namespace
	{
		/** The filters' weights of the sample before and the one before that, in 64ths. */
		constexpr std::array<int32, 5> OldWeights = {0, 60, 115, 98, 122};
		constexpr std::array<int32, 5> OlderWeights = {0, 0, -52, -55, -60};
		constexpr int32 LargestShift = 12;
		/** The shifts past 12 act as 9. */
		constexpr int32 ReservedShift = 9;
	}

	void DecodeSpuAdpcmBlock(std::span<const uint8> Block, std::span<int16> Out, int16& Old, int16& Older)
	{
		int32 Shift = Block[0] & 0x0F;
		if (Shift > LargestShift)
		{
			Shift = ReservedShift;
		}
		const std::size_t Filter = std::min<std::size_t>((Block[0] >> 4) & 7, OldWeights.size() - 1);
		for (int32 Index = 0; Index < SpuBlockSamples; ++Index)
		{
			const int32 Nibble = (Block[static_cast<std::size_t>(2 + Index / 2)] >> ((Index & 1) * 4)) & 0x0F;
			int32 Sample = static_cast<int16>(Nibble << 12) >> Shift;
			Sample += (Old * OldWeights[Filter] + Older * OlderWeights[Filter] + 32) >> 6;
			const int16 Clamped = static_cast<int16>(std::clamp(Sample, -0x8000, 0x7FFF));
			Older = Old;
			Old = Clamped;
			Out[static_cast<std::size_t>(Index)] = Clamped;
		}
	}
}
