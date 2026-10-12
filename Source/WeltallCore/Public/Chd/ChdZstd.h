#pragma once

#include "Chd/ChdTypes.h"

#include <memory>

namespace Chd
{
	namespace Zstd
	{
		class FBlockDecoder;
	}

	/** Decodes Zstandard frames (RFC 8878) made without a dictionary, as a CHD's zstd codecs pack hunks. */
	class FZstdDecoder
	{
	public:
		FZstdDecoder();
		~FZstdDecoder();
		FZstdDecoder(const FZstdDecoder&) = delete;
		FZstdDecoder& operator=(const FZstdDecoder&) = delete;

		/** Decodes Packed, whole frames, into exactly Destination; false when that is not what they hold. */
		bool Decode(std::span<const uint8> Packed, std::span<uint8> Destination);

	private:
		std::unique_ptr<Zstd::FBlockDecoder> Blocks;
	};
}
