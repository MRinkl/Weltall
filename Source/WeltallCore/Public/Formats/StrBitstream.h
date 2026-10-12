#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <span>
#include <vector>

namespace Weltall
{
	/**
	 * Expands a movie frame's compressed bitstream (version 2, as Sony's movie library writes it) into
	 * the MDEC's run-length codes for a Width x Height picture: per 8x8 block, the DC code (the
	 * frame's quantization scale and a 10-bit value), the coefficient codes, and the end-of-block code
	 * FE00h.
	 *
	 * The bitstream is an 8-byte header (a code count, 3800h, the quantization scale, the version)
	 * followed by little-endian halfwords read from their top bit down. Coefficients use the MPEG-1
	 * variable-length codes, with an escape to a literal 16-bit code.
	 */
	WELTALLCORE_API TResult<std::vector<uint16>> DecodeStrBitstream(std::span<const uint8> Frame, int32 Width, int32 Height);
}
