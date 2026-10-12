#include "Chd/ChdHuffman.h"

#include <algorithm>
#include <array>

namespace Chd
{
	FHuffmanDecoder::FHuffmanDecoder(uint32 InSymbolCount, uint32 InMaxBits)
		: SymbolCount(InSymbolCount), MaxBits(InMaxBits), FastBits(std::min(InMaxBits, MostFastBits)), Lengths(InSymbolCount),
		  Sorted(InSymbolCount), Fast(std::size_t{1} << FastBits)
	{
	}

	bool FHuffmanDecoder::ImportRle(FMsbBitReader& Reader)
	{
		const uint32 FieldBits = MaxBits >= 16 ? 5 : MaxBits >= 8 ? 4 : 3;
		for (uint32 Symbol = 0; Symbol < SymbolCount;)
		{
			const uint32 Length = Reader.Read(FieldBits);
			if (Length != 1)
			{
				Lengths[Symbol++] = static_cast<uint8>(Length);
				continue;
			}
			// A 1 escapes: 1 again is a length of 1, any other length comes with a count of repeats.
			const uint32 Repeated = Reader.Read(FieldBits);
			if (Repeated == 1)
			{
				Lengths[Symbol++] = 1;
				continue;
			}
			const uint32 Count = Reader.Read(FieldBits) + 3;
			if (Count > SymbolCount - Symbol)
			{
				return false;
			}
			std::fill_n(Lengths.begin() + Symbol, Count, static_cast<uint8>(Repeated));
			Symbol += Count;
		}
		return BuildCodes() && !Reader.Overran();
	}

	bool FHuffmanDecoder::ImportHuffman(FMsbBitReader& Reader)
	{
		// The small code's 24 symbols are a length plus one, or 0 for a run of the last length.
		FHuffmanDecoder Small(24, 6);
		Small.Lengths[0] = static_cast<uint8>(Reader.Read(3));
		const uint32 First = Reader.Read(3) + 1;
		bool bEnded = false;
		for (uint32 Symbol = 1; Symbol < Small.SymbolCount; ++Symbol)
		{
			if (Symbol < First || bEnded)
			{
				continue;
			}
			const uint32 Length = Reader.Read(3);
			bEnded = Length == 7;
			Small.Lengths[Symbol] = static_cast<uint8>(bEnded ? 0 : Length);
		}
		if (!Small.BuildCodes())
		{
			return false;
		}

		uint32 RunBits = 0;
		for (uint32 Rest = SymbolCount - 9; Rest != 0; Rest >>= 1)
		{
			++RunBits;
		}
		uint8 Last = 0;
		for (uint32 Symbol = 0; Symbol < SymbolCount;)
		{
			const uint32 Value = Small.Decode(Reader);
			if (Value == Invalid)
			{
				return false;
			}
			if (Value != 0)
			{
				Last = static_cast<uint8>(Value - 1);
				Lengths[Symbol++] = Last;
				continue;
			}
			uint32 Count = Reader.Read(3) + 2;
			if (Count == 9)
			{
				Count += Reader.Read(RunBits);
			}
			// A run past the last symbol ends with it.
			const uint32 Taken = std::min(Count, SymbolCount - Symbol);
			std::fill_n(Lengths.begin() + Symbol, Taken, Last);
			Symbol += Taken;
		}
		return BuildCodes() && !Reader.Overran();
	}

	bool FHuffmanDecoder::BuildCodes()
	{
		CodeCount.fill(0);
		for (const uint8 Length : Lengths)
		{
			if (Length > MaxBits)
			{
				return false;
			}
			++CodeCount[Length];
		}
		CodeCount[0] = 0;
		// The longest codes come first, numbered from zero; each shorter length starts where the longer ones end.
		uint32 Start = 0;
		for (uint32 Length = 32; Length > 0; --Length)
		{
			const uint32 End = Start + CodeCount[Length];
			if ((Length != 1 && End % 2 != 0) || (CodeCount[Length] != 0 && End > uint64{1} << Length))
			{
				return false;
			}
			FirstCode[Length] = Start;
			Start = End >> 1;
		}

		std::array<uint32, 33> Next{};
		for (uint32 Length = 1; Length < 32; ++Length)
		{
			FirstSorted[Length + 1] = FirstSorted[Length] + CodeCount[Length];
			Next[Length] = FirstSorted[Length];
		}
		Next[32] = FirstSorted[32];
		for (uint32 Symbol = 0; Symbol < SymbolCount; ++Symbol)
		{
			if (Lengths[Symbol] != 0)
			{
				Sorted[Next[Lengths[Symbol]]++] = static_cast<uint16>(Symbol);
			}
		}

		std::fill(Fast.begin(), Fast.end(), uint16{0});
		for (uint32 Length = 1; Length <= FastBits; ++Length)
		{
			const uint32 Shift = FastBits - Length;
			for (uint32 Index = 0; Index < CodeCount[Length]; ++Index)
			{
				const uint32 Code = FirstCode[Length] + Index;
				const uint16 Entry = static_cast<uint16>(Sorted[FirstSorted[Length] + Index] << LengthBits | Length);
				std::fill(Fast.begin() + (static_cast<std::ptrdiff_t>(Code) << Shift), Fast.begin() + (static_cast<std::ptrdiff_t>(Code + 1) << Shift), Entry);
			}
		}
		return true;
	}

	uint32 FHuffmanDecoder::DecodeLong(FMsbBitReader& Reader, uint32 Bits) const
	{
		for (uint32 Length = FastBits + 1; Length <= MaxBits; ++Length)
		{
			const uint32 Code = Bits >> (MaxBits - Length);
			if (Code - FirstCode[Length] < CodeCount[Length])
			{
				Reader.Skip(Length);
				return Sorted[FirstSorted[Length] + Code - FirstCode[Length]];
			}
		}
		return Invalid;
	}
}
