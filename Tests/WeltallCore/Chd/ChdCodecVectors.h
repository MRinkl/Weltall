#pragma once

#include "Chd/ChdTypes.h"

#include <array>
#include <string_view>

namespace Chd::Testing
{
	/** A stream packed by a reference encoder, and what it unpacks to. */
	struct FVector
	{
		std::string_view Name;
		uint64 OutputBytes;
		/** The output's FNV-1a hash, 64 bits. */
		uint64 OutputHash;
		/** The stream, two hexadecimal digits a byte. */
		std::string_view Hex;
		/** LZMA: lc, lp and pb. FLAC: 1 when the samples are wanted big-endian. */
		std::array<uint32, 3> Parameters;
	};

	/** Raw Deflate streams by zlib: empty, stored, fixed and dynamic codes, several blocks, far and long matches. */
	std::span<const FVector> DeflateVectors();

	/** Raw LZMA streams by liblzma, each ending with an end mark, with several lc, lp and pb. */
	std::span<const FVector> LzmaVectors();

	/**
	 * FLAC frames of 16-bit stereo by FFmpeg: every way of coding the two channels, fixed, high-order and no
	 * prediction, many partitions, constant subframes, several block sizes and sample rates.
	 */
	std::span<const FVector> FlacVectors();

	/**
	 * Zstandard frames by the reference tool: fast to ultra levels, with and without checksum and content size, raw,
	 * one-byte and compressed blocks, several blocks sharing tables, two frames around a skippable one.
	 */
	std::span<const FVector> ZstdVectors();
}
