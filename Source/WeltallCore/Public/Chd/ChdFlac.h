#pragma once

#include "Chd/ChdTypes.h"

#include <array>
#include <optional>
#include <vector>

namespace Chd
{
	/**
	 * Decodes FLAC frames (without the stream's header and metadata) of 16-bit stereo sound, as a CHD's flac codecs
	 * pack hunks: the samples interleaved, each in the byte order asked for.
	 */
	class FFlacDecoder
	{
	public:
		/**
		 * Decodes frames from the start of Packed until Destination, four bytes per stereo sample, is full. Returns how
		 * many bytes of Packed the frames took, or none when they are damaged or are not 16-bit stereo.
		 */
		std::optional<std::size_t> DecodeStereo16(std::span<const uint8> Packed, std::span<uint8> Destination, bool bBigEndian);

	private:
		/** One channel's samples of the frame being decoded. */
		std::array<std::vector<int32>, 2> Channels;
	};
}
