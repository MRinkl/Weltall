#pragma once

#include "Chd/ChdFlac.h"
#include "Chd/ChdHuffman.h"
#include "Chd/ChdInflate.h"
#include "Chd/ChdLzma.h"
#include "Chd/ChdTypes.h"
#include "Chd/ChdZstd.h"

#include <vector>

namespace Chd
{
	/**
	 * The working state of every codec, kept from one hunk to the next so that unpacking allocates nothing after the
	 * first hunks. One thread uses one at a time.
	 */
	class FHunkDecoder
	{
	public:
		FHunkDecoder();
		FHunkDecoder(const FHunkDecoder&) = delete;
		FHunkDecoder& operator=(const FHunkDecoder&) = delete;

		/** Unpacks Bytes, packed by the codec CodecTag names, into the whole of Destination; false when they do not unpack to that. */
		bool Decode(uint32 CodecTag, std::span<const uint8> Bytes, std::span<uint8> Destination);

		/** Room for a hunk's packed bytes. */
		std::vector<uint8>& PackedBuffer() { return Packed; }

	private:
		bool DecodeHuffman(std::span<const uint8> Bytes, std::span<uint8> Destination);
		bool DecodeFlac(std::span<const uint8> Bytes, std::span<uint8> Destination);

		/** A CD's frames: the sectors packed by one codec and their subchannel data by another, then put together. */
		bool DecodeCd(uint32 SectorCodec, uint32 SubchannelCodec, std::span<const uint8> Bytes, std::span<uint8> Destination);
		/** A CD's frames of sound: the sectors as FLAC, big-endian, then the subchannel data by Deflate. */
		bool DecodeCdFlac(std::span<const uint8> Bytes, std::span<uint8> Destination);
		/** Takes each frame's sector and subchannel data from where the CD codecs unpacked them apart. */
		void JoinCdFrames(std::span<uint8> Destination) const;

		std::vector<uint8> Packed;
		/** A CD hunk's sectors, then its subchannel data, as the CD codecs unpack them. */
		std::vector<uint8> CdParts;
		FInflater Inflater;
		FLzmaDecoder Lzma;
		FFlacDecoder Flac;
		FHuffmanDecoder Huffman;
		FZstdDecoder ZstdDecoder;
	};
}
