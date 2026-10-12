#pragma once

#include "Chd/ChdTypes.h"

#include <array>
#include <optional>
#include <vector>

/** Zstandard's entropy coding (RFC 8878, section 4): its bit streams, FSE tables and Huffman tables. */
namespace Chd::Zstd
{
	/**
	 * A bit stream read from its end towards its start, as Zstandard writes its entropy-coded streams: the last byte's
	 * highest set bit marks where the bits begin. Past the start it reads zeros and remembers that it ran over.
	 */
	class FBackwardBits
	{
	public:
		/** False when the last byte holds no start mark. */
		bool Begin(std::span<const uint8> InBytes);

		/** The next Count bits (at most 32), the first read the highest, without taking them. */
		uint32 Peek(uint32 Count) const;

		void Skip(uint32 Count) { Position -= static_cast<int64>(Count); }

		uint32 Read(uint32 Count)
		{
			const uint32 Value = Peek(Count);
			Skip(Count);
			return Value;
		}

		/** Whether every bit has been read, and no more. */
		bool IsDone() const { return Position == 0; }
		bool Overran() const { return Position < 0; }

	private:
		std::span<const uint8> Bytes;
		/** The bits below this one are still to be read. */
		int64 Position = 0;
	};

	/** One state of an FSE decoding table: the symbol it gives, and how the next state is reached from it. */
	struct FFseEntry
	{
		uint16 Baseline = 0;
		uint8 Symbol = 0;
		uint8 Bits = 0;
	};

	/** An FSE decoding table, 2^AccuracyLog states. */
	struct FFseTable
	{
		uint32 AccuracyLog = 0;
		std::vector<FFseEntry> States;

		/** Builds the table from each symbol's normalized count, -1 for "less than one"; false when they do not fill it. */
		bool Build(std::span<const int16> Counts, uint32 InAccuracyLog);

		/** A table whose every state gives Symbol and reads no bits. */
		void BuildRle(uint8 Symbol);
	};

	/**
	 * Reads an FSE table's description (its accuracy and normalized counts) from the start of Bytes and builds it;
	 * returns the bytes it took, or none when it is no table of at most MaxSymbol + 1 symbols and MaxAccuracyLog.
	 */
	std::optional<std::size_t> ReadFseTable(std::span<const uint8> Bytes, uint32 MaxSymbol, uint32 MaxAccuracyLog, FFseTable& Table);

	/** One state of the decoding: its symbol, read through the bit stream. */
	class FFseState
	{
	public:
		void Begin(const FFseTable& InTable, FBackwardBits& Bits)
		{
			Table = &InTable;
			State = Bits.Read(Table->AccuracyLog);
		}

		uint8 Symbol() const { return Table->States[State].Symbol; }

		void Update(FBackwardBits& Bits)
		{
			const FFseEntry& Entry = Table->States[State];
			State = Entry.Baseline + Bits.Read(Entry.Bits);
		}

	private:
		const FFseTable* Table = nullptr;
		uint32 State = 0;
	};

	/** The Huffman code of a block's literals: a table of every MaxBits-bit value read. */
	struct FHuffmanTable
	{
		static constexpr uint32 MostBits = 11;

		uint32 MaxBits = 0;
		/** For each MaxBits-bit value, the symbol whose code begins it and that code's length. */
		std::vector<uint8> Symbols;
		std::vector<uint8> Lengths;

		/** The next literal from a stream. */
		uint8 Decode(FBackwardBits& Bits) const
		{
			const uint32 Index = Bits.Peek(MaxBits);
			Bits.Skip(Lengths[Index]);
			return Symbols[Index];
		}
	};

	/**
	 * Reads a Huffman tree's description (its symbols' weights, plain or FSE-coded) from the start of Bytes and builds
	 * the table; returns the bytes it took, or none when it describes no code.
	 */
	std::optional<std::size_t> ReadHuffmanTable(std::span<const uint8> Bytes, FHuffmanTable& Table);
}
