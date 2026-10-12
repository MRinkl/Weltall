#pragma once

#include "Foundation/Types.h"

#include <span>

namespace Weltall
{
	/** An SPU-ADPCM block holds 28 samples in 16 bytes: its header, its flags, then two nibbles a byte. */
	inline constexpr int32 SpuBlockSamples = 28;
	inline constexpr int32 SpuBlockBytes = 16;

	/** The flags in a block's second byte. */
	inline constexpr uint8 SpuLoopEnd = 1;
	inline constexpr uint8 SpuLoopRepeat = 2;
	inline constexpr uint8 SpuLoopStart = 4;

	/**
	 * Decodes a block's samples into Out (28 of them), the filter reading and carrying the two samples
	 * before, which run on from block to block.
	 */
	void DecodeSpuAdpcmBlock(std::span<const uint8> Block, std::span<int16> Out, int16& Old, int16& Older);
}
