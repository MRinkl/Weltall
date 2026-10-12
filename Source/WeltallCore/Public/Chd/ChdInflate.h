#pragma once

#include "Chd/ChdTypes.h"

#include <array>

namespace Chd
{
	/** A Huffman code of Deflate's, decoded through a table of its short codes. */
	class FDeflateCode
	{
	public:
		static constexpr uint32 MaxBits = 15;
		static constexpr uint32 Invalid = 0xFFFF;

		/**
		 * Builds the code from each symbol's code length (0 for an unused symbol). False when the lengths are no
		 * prefix code: too many codes for a length, or too few to fill the code, except a single code of one bit.
		 */
		bool Build(std::span<const uint8> Lengths);

		/** The next symbol, or Invalid where the bits begin no code. */
		template <typename TReader>
		uint32 Decode(TReader& Reader) const
		{
			const uint32 Bits = Reader.Peek(MaxBits);
			const uint16 Entry = Fast[Bits & (FastSize - 1)];
			if (Entry != 0)
			{
				Reader.Skip(Entry & 0xF);
				return Entry >> 4;
			}
			return DecodeLong(Reader, Bits);
		}

	private:
		static constexpr uint32 FastBits = 10;
		static constexpr uint32 FastSize = 1u << FastBits;

		/** A code longer than the table's, found canonically one bit at a time from the first. */
		template <typename TReader>
		uint32 DecodeLong(TReader& Reader, uint32 Bits) const
		{
			uint32 Code = 0;
			uint32 First = 0;
			uint32 Index = 0;
			for (uint32 Length = 1; Length <= MaxBits; ++Length)
			{
				Code |= Bits >> (Length - 1) & 1;
				const uint32 Count = Counts[Length];
				if (Code - First < Count)
				{
					Reader.Skip(Length);
					return Symbols[Index + Code - First];
				}
				Index += Count;
				First = (First + Count) << 1;
				Code <<= 1;
			}
			return Invalid;
		}

		/** For each FastBits-bit value read, the symbol whose short code begins it and the code's length; 0 for none. */
		std::array<uint16, FastSize> Fast{};
		std::array<uint16, MaxBits + 1> Counts{};
		/** The symbols in the order of their codes. */
		std::array<uint16, 288> Symbols{};
	};

	/** Unpacks raw Deflate streams (RFC 1951), as a CHD's zlib codec packs its hunks. */
	class FInflater
	{
	public:
		FInflater();

		/** Unpacks Packed, which must be a whole stream of exactly Destination's size; false when it is not. */
		bool Inflate(std::span<const uint8> Packed, std::span<uint8> Destination);

	private:
		FDeflateCode FixedLiterals;
		FDeflateCode FixedDistances;
		FDeflateCode Literals;
		FDeflateCode Distances;
	};
}
