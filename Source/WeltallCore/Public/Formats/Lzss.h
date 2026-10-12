#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <span>
#include <vector>

namespace Weltall
{
	/**
	 * Unpacks the game's LZSS: a 32-bit output size, then groups of a flag byte and eight items, from
	 * the flag's lowest bit up, a clear bit a literal byte and a set bit a 2-byte back-reference.
	 *
	 * A stream can finish in the zero padding after its file's recorded size, so pass the file's
	 * sectors in full. Unlike the game, which trusts its data, a stream that runs past its input,
	 * overshoots its size or refers outside the output written so far is reported as an error.
	 */
	WELTALLCORE_API TResult<std::vector<uint8>> UnpackLzss(std::span<const uint8> Stream);

	/**
	 * The first Count bytes a stream unpacks to, for data whose own size is recorded elsewhere and
	 * is smaller than the stream's. Decoding stops as soon as they are out, so whatever the stream
	 * holds after them is neither read nor checked.
	 */
	WELTALLCORE_API TResult<std::vector<uint8>> UnpackLzssPrefix(std::span<const uint8> Stream, uint32 Count);
}
