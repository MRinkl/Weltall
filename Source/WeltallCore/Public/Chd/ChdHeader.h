#pragma once

#include "Chd/ChdTypes.h"

#include <array>

namespace Chd
{
	/** Four letters as one big-endian number: how a CHD names its codecs and metadata. */
	constexpr uint32 MakeTag(char A, char B, char C, char D)
	{
		return static_cast<uint32>(static_cast<uint8>(A)) << 24 | static_cast<uint32>(static_cast<uint8>(B)) << 16
			| static_cast<uint32>(static_cast<uint8>(C)) << 8 | static_cast<uint32>(static_cast<uint8>(D));
	}

	/** The codecs a CHD's hunks may be packed with. */
	namespace Codec
	{
		inline constexpr uint32 None = 0;
		inline constexpr uint32 Zlib = MakeTag('z', 'l', 'i', 'b');
		inline constexpr uint32 Lzma = MakeTag('l', 'z', 'm', 'a');
		inline constexpr uint32 Huffman = MakeTag('h', 'u', 'f', 'f');
		inline constexpr uint32 Flac = MakeTag('f', 'l', 'a', 'c');
		inline constexpr uint32 Zstd = MakeTag('z', 's', 't', 'd');
		/** A CD's frames: the sectors packed by the named codec, their subchannel data apart. */
		inline constexpr uint32 CdZlib = MakeTag('c', 'd', 'z', 'l');
		inline constexpr uint32 CdLzma = MakeTag('c', 'd', 'l', 'z');
		inline constexpr uint32 CdFlac = MakeTag('c', 'd', 'f', 'l');
		inline constexpr uint32 CdZstd = MakeTag('c', 'd', 'z', 's');
	}

	using FSha1Digest = std::array<uint8, 20>;

	/** What a CHD's header says of it (version 5, the only one read). */
	struct FHeader
	{
		static constexpr uint32 Bytes = 124;

		/** Up to four codecs, which a hunk names by their place here; None for the unused places. */
		std::array<uint32, 4> Codecs{};
		/** The size of the image packed: its units one after another. */
		uint64 LogicalBytes = 0;
		uint64 MapOffset = 0;
		/** Where the first metadata entry is in the file; 0 for none. */
		uint64 MetadataOffset = 0;
		/** The size each hunk unpacks to: a whole number of units. */
		uint32 HunkBytes = 0;
		/** The size of the image's own unit: a DVD's sector, or a CD's sector with its subchannel data. */
		uint32 UnitBytes = 0;
		/** The SHA-1 of the image's logical bytes. */
		FSha1Digest RawSha1{};
		/** The SHA-1 of the logical bytes together with the metadata. */
		FSha1Digest Sha1{};
		/** The SHA-1 of the parent this CHD holds the differences from; all zeros for none. */
		FSha1Digest ParentSha1{};

		uint32 HunkCount() const { return static_cast<uint32>((LogicalBytes + HunkBytes - 1) / HunkBytes); }

		/** Whether the hunks are packed at all: an unpacked CHD keeps a plain map. */
		bool IsCompressed() const { return Codecs[0] != Codec::None; }

		bool HasParent() const;
	};

	/** Reads a CHD's header and checks that it describes a version 5 CHD that can be read. */
	TResult<FHeader> ReadHeader(const IInput& Input);

	/** Whether the codec is one whose hunks this reader unpacks. */
	bool IsKnownCodec(uint32 Tag);
}
