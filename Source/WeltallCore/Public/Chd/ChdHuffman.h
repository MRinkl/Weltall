#pragma once

#include "Chd/ChdBitReader.h"
#include "Chd/ChdTypes.h"

#include <array>
#include <vector>

namespace Chd
{
	/**
	 * MAME's static Huffman code, which a CHD uses for its map and as one of its codecs. Only the code lengths are
	 * stored, in one of two ways; the codes follow from them canonically, the longest codes numbered from zero.
	 */
	class FHuffmanDecoder
	{
	public:
		static constexpr uint32 Invalid = 0xFFFFFFFF;

		FHuffmanDecoder(uint32 InSymbolCount, uint32 InMaxBits);

		/** Reads code lengths stored with runs of equal lengths escaped by a length of 1; false when they make no code. */
		bool ImportRle(FMsbBitReader& Reader);

		/** Reads code lengths coded by a small Huffman code of their own, with runs; false when they make no code. */
		bool ImportHuffman(FMsbBitReader& Reader);

		/** The next symbol, or Invalid where the bits begin no code. */
		uint32 Decode(FMsbBitReader& Reader) const
		{
			const uint32 Bits = Reader.Peek(MaxBits);
			const uint16 Entry = Fast[Bits >> (MaxBits - FastBits)];
			if (Entry != 0)
			{
				Reader.Skip(Entry & LengthMask);
				return Entry >> LengthBits;
			}
			return DecodeLong(Reader, Bits);
		}

	private:
		static constexpr uint32 MostFastBits = 11;
		static constexpr uint32 LengthBits = 5;
		static constexpr uint16 LengthMask = (1u << LengthBits) - 1;

		/** Gives each symbol its code from the lengths and fills the table of the short codes. */
		bool BuildCodes();

		/** A code longer than the table's: the one length whose codes the bits begin with. */
		uint32 DecodeLong(FMsbBitReader& Reader, uint32 Bits) const;

		uint32 SymbolCount;
		uint32 MaxBits;
		uint32 FastBits;
		std::vector<uint8> Lengths;
		/** Per length, its first code, how many codes it has and where its symbols begin among the sorted ones. */
		std::array<uint32, 33> FirstCode{};
		std::array<uint32, 33> CodeCount{};
		std::array<uint32, 33> FirstSorted{};
		/** The symbols by length, then by value: the order their codes count up in within each length. */
		std::vector<uint16> Sorted;
		/** For each FastBits-bit value, the symbol whose short code begins it and that code's length; 0 where none does. */
		std::vector<uint16> Fast;
	};
}
