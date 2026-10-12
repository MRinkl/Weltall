#pragma once

#include "Chd/ChdZstdEntropy.h"

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

/** Zstandard's blocks (RFC 8878, section 3.1.1.3): their literals and the sequences that place them and their matches. */
namespace Chd::Zstd
{
	/** Size little-endian bytes from At on as a number; the caller guarantees the bytes exist. */
	inline uint64 ReadLittle(std::span<const uint8> Bytes, std::size_t At, std::size_t Size)
	{
		uint64 Value = 0;
		for (std::size_t Index = 0; Index < Size; ++Index)
		{
			Value |= static_cast<uint64>(Bytes[At + Index]) << (8 * Index);
		}
		return Value;
	}

	/** Where a frame's output goes: the destination, from the frame's first byte on. */
	class FFrameOutput
	{
	public:
		FFrameOutput(std::span<uint8> InBytes, std::size_t InStart) : Bytes(InBytes), Start(InStart), Position(InStart) {}

		bool Put(std::span<const uint8> Data)
		{
			if (Data.size() > Bytes.size() - Position)
			{
				return false;
			}
			std::copy(Data.begin(), Data.end(), Bytes.begin() + static_cast<std::ptrdiff_t>(Position));
			Position += Data.size();
			return true;
		}

		bool Fill(uint8 Value, std::size_t Count)
		{
			if (Count > Bytes.size() - Position)
			{
				return false;
			}
			std::fill_n(Bytes.begin() + static_cast<std::ptrdiff_t>(Position), Count, Value);
			Position += Count;
			return true;
		}

		/** Repeats Length bytes from Distance back within the frame, which may overlap what it writes. */
		bool Copy(std::size_t Distance, std::size_t Length)
		{
			if (Distance == 0 || Distance > Position - Start || Length > Bytes.size() - Position)
			{
				return false;
			}
			for (std::size_t From = Position - Distance, End = Position + Length; Position < End;)
			{
				Bytes[Position++] = Bytes[From++];
			}
			return true;
		}

		std::span<const uint8> Written() const { return std::span<const uint8>(Bytes).subspan(Start, Position - Start); }
		std::size_t End() const { return Position; }

	private:
		std::span<uint8> Bytes;
		std::size_t Start;
		std::size_t Position;
	};

	/** Decodes a frame's compressed blocks, keeping what each hands on to the next: the entropy tables and the offsets last used. */
	class FBlockDecoder
	{
	public:
		FBlockDecoder();

		/** Forgets the tables and offsets of the frame before. */
		void BeginFrame();

		/** Decodes a compressed block into Output; false when it is damaged. */
		bool Decode(std::span<const uint8> Bytes, FFrameOutput& Output);

	private:
		/** Reads the literals section into Literals; returns the bytes it took. */
		std::optional<std::size_t> ReadLiterals(std::span<const uint8> Bytes);
		bool DecodeStreams(std::span<const uint8> Bytes, std::size_t Count, bool bFourStreams);
		/** Reads the sequences and carries them out: literals, then a match, until the literals left end the block. */
		bool Sequences(std::span<const uint8> Bytes, FFrameOutput& Output);

		std::array<FFseTable, 3> Defaults;
		std::array<FFseTable, 3> Tables;
		std::array<bool, 3> bHasTable{};
		FHuffmanTable Huffman;
		bool bHasHuffman = false;
		std::array<uint32, 3> Repeats{};
		std::vector<uint8> Literals;
	};
}
