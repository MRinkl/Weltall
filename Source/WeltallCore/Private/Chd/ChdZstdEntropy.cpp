#include "Chd/ChdZstdEntropy.h"

#include <algorithm>
#include <bit>
#include <cstdlib>

namespace Chd::Zstd
{
	namespace
	{
		uint32 HighBit(uint32 Value)
		{
			return 31 - static_cast<uint32>(std::countl_zero(Value));
		}

		/** Up to eight bytes from At on, little-endian, zeros past the end. */
		uint64 LoadLittle(std::span<const uint8> Bytes, std::size_t At)
		{
			uint64 Value = 0;
			for (std::size_t Index = 0; Index < 8 && At + Index < Bytes.size(); ++Index)
			{
				Value |= static_cast<uint64>(Bytes[At + Index]) << (8 * Index);
			}
			return Value;
		}

		/** Bits read from the start, the lowest of each byte first: how an FSE table's description is written. */
		class FForwardBits
		{
		public:
			explicit FForwardBits(std::span<const uint8> InBytes) : Bytes(InBytes) {}

			uint32 Peek(uint32 Count) const
			{
				return static_cast<uint32>(LoadLittle(Bytes, Position / 8) >> (Position % 8) & ((uint64{1} << Count) - 1));
			}

			void Skip(uint32 Count) { Position += Count; }

			uint32 Read(uint32 Count)
			{
				const uint32 Value = Peek(Count);
				Skip(Count);
				return Value;
			}

			std::size_t BytesTaken() const { return (Position + 7) / 8; }

		private:
			std::span<const uint8> Bytes;
			std::size_t Position = 0;
		};

		constexpr uint32 MostHuffmanWeights = 255;
		constexpr uint32 HuffmanWeightAccuracyLog = 6;
	}

	bool FBackwardBits::Begin(std::span<const uint8> InBytes)
	{
		Bytes = InBytes;
		if (Bytes.empty() || Bytes.back() == 0)
		{
			return false;
		}
		Position = static_cast<int64>((Bytes.size() - 1) * 8 + HighBit(Bytes.back()));
		return true;
	}

	uint32 FBackwardBits::Peek(uint32 Count) const
	{
		if (Count == 0 || Position <= 0)
		{
			return 0;
		}
		const int64 Low = Position - Count;
		if (Low >= 0)
		{
			return static_cast<uint32>(LoadLittle(Bytes, static_cast<std::size_t>(Low / 8)) >> (Low % 8) & ((uint64{1} << Count) - 1));
		}
		// Past the start the bits are zeros, below those that remain.
		const uint64 Remaining = LoadLittle(Bytes, 0) & ((uint64{1} << Position) - 1);
		return static_cast<uint32>(Remaining << -Low);
	}

	bool FFseTable::Build(std::span<const int16> Counts, uint32 InAccuracyLog)
	{
		AccuracyLog = InAccuracyLog;
		const uint32 Size = 1u << AccuracyLog;
		States.assign(Size, FFseEntry());
		if (Counts.size() > 256)
		{
			return false;
		}
		uint32 Total = 0;
		for (const int16 Count : Counts)
		{
			Total += Count == -1 ? 1 : static_cast<uint32>(std::max<int16>(Count, 0));
		}
		if (Total != Size)
		{
			return false;
		}
		// Symbols less likely than one in the table's size take one state each, at its end.
		std::array<uint32, 256> Next{};
		uint32 HighThreshold = Size - 1;
		for (uint32 Symbol = 0; Symbol < Counts.size(); ++Symbol)
		{
			if (Counts[Symbol] == -1)
			{
				States[HighThreshold--].Symbol = static_cast<uint8>(Symbol);
				Next[Symbol] = 1;
			}
			else if (Counts[Symbol] > 0)
			{
				Next[Symbol] = static_cast<uint32>(Counts[Symbol]);
			}
		}
		// The others are spread over the rest of the table with a step that visits every state once.
		const uint32 Step = (Size >> 1) + (Size >> 3) + 3;
		uint32 Position = 0;
		for (uint32 Symbol = 0; Symbol < Counts.size(); ++Symbol)
		{
			for (int32 Each = 0; Each < Counts[Symbol]; ++Each)
			{
				States[Position].Symbol = static_cast<uint8>(Symbol);
				do
				{
					Position = (Position + Step) & (Size - 1);
				} while (Position > HighThreshold);
			}
		}
		if (Position != 0)
		{
			return false;
		}
		for (FFseEntry& State : States)
		{
			const uint32 NextState = Next[State.Symbol]++;
			State.Bits = static_cast<uint8>(AccuracyLog - HighBit(NextState));
			State.Baseline = static_cast<uint16>((NextState << State.Bits) - Size);
		}
		return true;
	}

	void FFseTable::BuildRle(uint8 Symbol)
	{
		AccuracyLog = 0;
		States.assign(1, FFseEntry{0, Symbol, 0});
	}

	std::optional<std::size_t> ReadFseTable(std::span<const uint8> Bytes, uint32 MaxSymbol, uint32 MaxAccuracyLog, FFseTable& Table)
	{
		FForwardBits Bits(Bytes);
		const uint32 AccuracyLog = Bits.Read(4) + 5;
		if (AccuracyLog > MaxAccuracyLog)
		{
			return std::nullopt;
		}
		std::array<int16, 256> Counts{};
		int32 Remaining = (1 << AccuracyLog) + 1;
		int32 Threshold = 1 << AccuracyLog;
		uint32 CountBits = AccuracyLog + 1;
		uint32 Symbol = 0;
		bool bAfterZero = false;
		while (Remaining > 1 && Symbol <= MaxSymbol)
		{
			if (bAfterZero)
			{
				// Two-bit flags count more zeros; a 3 says another flag follows.
				uint32 Zeros = 0;
				for (uint32 Flag = 3; Flag == 3;)
				{
					Flag = Bits.Read(2);
					Zeros += Flag;
				}
				if (Symbol + Zeros > MaxSymbol)
				{
					return std::nullopt;
				}
				Symbol += Zeros;
			}
			// A count below the code's small values takes one bit less.
			const int32 Max = 2 * Threshold - 1 - Remaining;
			int32 Count = 0;
			if (static_cast<int32>(Bits.Peek(CountBits - 1)) < Max)
			{
				Count = static_cast<int32>(Bits.Read(CountBits - 1));
			}
			else
			{
				Count = static_cast<int32>(Bits.Read(CountBits));
				if (Count >= Threshold)
				{
					Count -= Max;
				}
			}
			--Count;
			Remaining -= std::abs(Count);
			if (Remaining < 1)
			{
				return std::nullopt;
			}
			Counts[Symbol++] = static_cast<int16>(Count);
			bAfterZero = Count == 0;
			while (Remaining < Threshold)
			{
				--CountBits;
				Threshold >>= 1;
			}
		}
		if (Remaining != 1 || Bits.BytesTaken() > Bytes.size() || !Table.Build(std::span(Counts).first(Symbol), AccuracyLog))
		{
			return std::nullopt;
		}
		return Bits.BytesTaken();
	}

	std::optional<std::size_t> ReadHuffmanTable(std::span<const uint8> Bytes, FHuffmanTable& Table)
	{
		if (Bytes.empty())
		{
			return std::nullopt;
		}
		std::array<uint8, MostHuffmanWeights + 1> Weights{};
		uint32 WeightCount = 0;
		std::size_t Taken = 0;
		const uint32 Header = Bytes[0];
		if (Header >= 128)
		{
			// Four bits a weight, the first in the high half of each byte.
			WeightCount = Header - 127;
			Taken = 1 + (WeightCount + 1) / 2;
			if (Taken > Bytes.size())
			{
				return std::nullopt;
			}
			for (uint32 Index = 0; Index < WeightCount; ++Index)
			{
				const uint8 Pair = Bytes[1 + Index / 2];
				Weights[Index] = static_cast<uint8>(Index % 2 == 0 ? Pair >> 4 : Pair & 0xF);
			}
		}
		else
		{
			// The weights coded by FSE, with two states taking turns until the bits run out.
			Taken = 1 + Header;
			if (Taken > Bytes.size())
			{
				return std::nullopt;
			}
			const std::span<const uint8> Coded = Bytes.subspan(1, Header);
			FFseTable WeightTable;
			const auto TableBytes = ReadFseTable(Coded, MostHuffmanWeights, HuffmanWeightAccuracyLog, WeightTable);
			FBackwardBits Bits;
			if (!TableBytes || !Bits.Begin(Coded.subspan(*TableBytes)))
			{
				return std::nullopt;
			}
			std::array<FFseState, 2> States;
			States[0].Begin(WeightTable, Bits);
			States[1].Begin(WeightTable, Bits);
			for (uint32 Turn = 0;; Turn ^= 1)
			{
				if (WeightCount + 2 > MostHuffmanWeights)
				{
					return std::nullopt;
				}
				Weights[WeightCount++] = States[Turn].Symbol();
				States[Turn].Update(Bits);
				if (Bits.Overran())
				{
					Weights[WeightCount++] = States[Turn ^ 1].Symbol();
					break;
				}
			}
		}

		// The last symbol's weight is not written: it is what makes the weights' sum a power of two.
		uint32 Total = 0;
		std::array<uint32, FHuffmanTable::MostBits + 2> PerWeight{};
		for (uint32 Index = 0; Index < WeightCount; ++Index)
		{
			if (Weights[Index] > FHuffmanTable::MostBits)
			{
				return std::nullopt;
			}
			Total += Weights[Index] == 0 ? 0 : 1u << (Weights[Index] - 1);
			++PerWeight[Weights[Index]];
		}
		if (Total == 0)
		{
			return std::nullopt;
		}
		const uint32 MaxBits = HighBit(Total) + 1;
		const uint32 Rest = (1u << MaxBits) - Total;
		if (MaxBits > FHuffmanTable::MostBits || !std::has_single_bit(Rest))
		{
			return std::nullopt;
		}
		Weights[WeightCount] = static_cast<uint8>(HighBit(Rest) + 1);
		++PerWeight[Weights[WeightCount]];
		++WeightCount;
		// A Huffman tree has an even number of leaves on its deepest level, at least two.
		if (PerWeight[1] < 2 || PerWeight[1] % 2 != 0)
		{
			return std::nullopt;
		}

		// Codes are given from the lowest weight up, each weight's symbols in their order, longer codes first.
		Table.MaxBits = MaxBits;
		Table.Symbols.assign(std::size_t{1} << MaxBits, 0);
		Table.Lengths.assign(std::size_t{1} << MaxBits, 0);
		std::size_t Position = 0;
		for (uint32 Weight = 1; Weight <= MaxBits; ++Weight)
		{
			for (uint32 Symbol = 0; Symbol < WeightCount; ++Symbol)
			{
				if (Weights[Symbol] != Weight)
				{
					continue;
				}
				const std::size_t Entries = std::size_t{1} << (Weight - 1);
				std::fill_n(Table.Symbols.begin() + static_cast<std::ptrdiff_t>(Position), Entries, static_cast<uint8>(Symbol));
				std::fill_n(Table.Lengths.begin() + static_cast<std::ptrdiff_t>(Position), Entries, static_cast<uint8>(MaxBits + 1 - Weight));
				Position += Entries;
			}
		}
		return Taken;
	}
}
