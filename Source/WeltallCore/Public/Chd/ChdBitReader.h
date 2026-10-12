#pragma once

#include "Chd/ChdTypes.h"

namespace Chd
{
	/**
	 * Bits read most significant first, as a CHD's map and its Huffman codec store them. Past the end of its bytes it
	 * reads zeros and remembers that it ran over.
	 */
	class FMsbBitReader
	{
	public:
		explicit FMsbBitReader(std::span<const uint8> InBytes) : Bytes(InBytes) {}

		/** The next Count bits (at most 32) without taking them. */
		uint32 Peek(uint32 Count)
		{
			while (Buffered < Count)
			{
				const uint64 Byte = NextByte < Bytes.size() ? Bytes[NextByte] : 0;
				Buffer |= Byte << (56 - Buffered);
				++NextByte;
				Buffered += 8;
			}
			return Count == 0 ? 0 : static_cast<uint32>(Buffer >> (64 - Count));
		}

		void Skip(uint32 Count)
		{
			Buffer <<= Count;
			Buffered -= Count;
		}

		uint32 Read(uint32 Count)
		{
			const uint32 Value = Peek(Count);
			Skip(Count);
			return Value;
		}

		/** Whether more bits have been taken than the bytes hold. */
		bool Overran() const { return NextByte * 8 - Buffered > Bytes.size() * 8; }

	private:
		std::span<const uint8> Bytes;
		std::size_t NextByte = 0;
		uint64 Buffer = 0;
		uint32 Buffered = 0;
	};

	/**
	 * Bits read least significant first, as Deflate stores them. Past the end of its bytes it reads zeros and
	 * remembers that it ran over.
	 */
	class FLsbBitReader
	{
	public:
		explicit FLsbBitReader(std::span<const uint8> InBytes) : Bytes(InBytes) {}

		/** The next Count bits (at most 32) without taking them, the first in the lowest bit. */
		uint32 Peek(uint32 Count)
		{
			while (Buffered < Count)
			{
				const uint64 Byte = NextByte < Bytes.size() ? Bytes[NextByte] : 0;
				Buffer |= Byte << Buffered;
				++NextByte;
				Buffered += 8;
			}
			return static_cast<uint32>(Buffer & ((uint64{1} << Count) - 1));
		}

		void Skip(uint32 Count)
		{
			Buffer >>= Count;
			Buffered -= Count;
		}

		uint32 Read(uint32 Count)
		{
			const uint32 Value = Peek(Count);
			Skip(Count);
			return Value;
		}

		/** Drops the bits up to the next whole byte. */
		void SkipToByte() { Skip(Buffered % 8); }

		/** Whether more bits have been taken than the bytes hold. */
		bool Overran() const { return NextByte * 8 - Buffered > Bytes.size() * 8; }

	private:
		std::span<const uint8> Bytes;
		std::size_t NextByte = 0;
		uint64 Buffer = 0;
		uint32 Buffered = 0;
	};
}
