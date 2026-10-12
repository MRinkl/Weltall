#pragma once

#include "Chd/ChdHeader.h"
#include "Chd/ChdTypes.h"

#include <vector>

namespace Chd
{
	/** How a hunk is kept in the file. */
	enum class EHunkKind : uint8
	{
		/** Packed by the header's first, second, third or fourth codec. */
		Codec0,
		Codec1,
		Codec2,
		Codec3,
		/** Kept as it is, with its CRC. */
		Stored,
		/** The same as an earlier hunk: Source() numbers it. */
		Copy,
		/** Kept as it is, without a CRC: the hunks of a CHD that packs none. */
		Plain,
		/** Never written: zeros. */
		Zeros,
	};

	/** Where one hunk's data lies and how it is kept, in twelve bytes: a disc has millions of hunks. */
	class FHunkEntry
	{
	public:
		FHunkEntry() = default;
		FHunkEntry(EHunkKind InKind, uint64 InOffset, uint32 InLength, uint16 InCrc)
			: OffsetLow(static_cast<uint32>(InOffset)), OffsetHigh(static_cast<uint16>(InOffset >> 32)), Crc(InCrc),
			  LengthAndKind(InLength << 8 | static_cast<uint32>(InKind))
		{
		}

		EHunkKind Kind() const { return static_cast<EHunkKind>(LengthAndKind & 0xFF); }
		/** Where the hunk's data begins in the file. */
		uint64 Offset() const { return static_cast<uint64>(OffsetHigh) << 32 | OffsetLow; }
		/** A copy: the hunk it is a copy of, never itself a copy. */
		uint32 Source() const { return OffsetLow; }
		/** How many bytes of the file the hunk's data takes. */
		uint32 Length() const { return LengthAndKind >> 8; }
		/** The CRC-16 of the hunk unpacked, where the kind keeps one. */
		uint16 ExpectedCrc() const { return Crc; }

	private:
		uint32 OffsetLow = 0;
		uint16 OffsetHigh = 0;
		uint16 Crc = 0;
		uint32 LengthAndKind = 0;
	};

	/**
	 * Reads the map of every hunk: for a packed CHD, Huffman-coded kinds followed by each hunk's length and CRC, checked
	 * by the map's own CRC. Every hunk's data must lie within the file, and every copy name an earlier hunk.
	 */
	TResult<std::vector<FHunkEntry>> ReadMap(const IInput& Input, const FHeader& Header);
}
