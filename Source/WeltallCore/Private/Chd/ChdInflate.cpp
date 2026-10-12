#include "Chd/ChdInflate.h"

#include "Chd/ChdBitReader.h"

#include <algorithm>

namespace Chd
{
	namespace
	{
		constexpr uint32 EndOfBlock = 256;
		constexpr uint32 LiteralCodes = 288;
		constexpr uint32 DistanceCodes = 32;

		/** Lengths 3 to 258 of codes 257 to 285: the base, and the extra bits that add to it. */
		constexpr std::array<uint16, 29> LengthBase{3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
		constexpr std::array<uint8, 29> LengthExtra{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
		constexpr std::array<uint16, 30> DistanceBase{1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537,
			2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
		constexpr std::array<uint8, 30> DistanceExtra{0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

		/** The order a dynamic block lists the code lengths' own code lengths in. */
		constexpr std::array<uint8, 19> CodeLengthOrder{16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

		uint32 Reverse(uint32 Code, uint32 Length)
		{
			uint32 Reversed = 0;
			for (uint32 Bit = 0; Bit < Length; ++Bit)
			{
				Reversed = Reversed << 1 | (Code >> Bit & 1);
			}
			return Reversed;
		}

		class FOutput
		{
		public:
			explicit FOutput(std::span<uint8> InBytes) : Bytes(InBytes) {}

			bool Put(uint8 Byte)
			{
				if (Position == Bytes.size())
				{
					return false;
				}
				Bytes[Position++] = Byte;
				return true;
			}

			/** Repeats Length bytes from Distance back, which may overlap what it writes. */
			bool Copy(uint32 Distance, uint32 Length)
			{
				if (Distance > Position || Length > Bytes.size() - Position)
				{
					return false;
				}
				for (std::size_t From = Position - Distance, End = Position + Length; Position < End;)
				{
					Bytes[Position++] = Bytes[From++];
				}
				return true;
			}

			bool IsFull() const { return Position == Bytes.size(); }

		private:
			std::span<uint8> Bytes;
			std::size_t Position = 0;
		};

		bool Stored(FLsbBitReader& Reader, FOutput& Output)
		{
			Reader.SkipToByte();
			const uint32 Length = Reader.Read(16);
			if ((Reader.Read(16) ^ 0xFFFF) != Length)
			{
				return false;
			}
			for (uint32 Index = 0; Index < Length; ++Index)
			{
				if (!Output.Put(static_cast<uint8>(Reader.Read(8))))
				{
					return false;
				}
			}
			return true;
		}

		bool Codes(FLsbBitReader& Reader, FOutput& Output, const FDeflateCode& Literals, const FDeflateCode& Distances)
		{
			for (;;)
			{
				const uint32 Symbol = Literals.Decode(Reader);
				if (Symbol < EndOfBlock)
				{
					if (!Output.Put(static_cast<uint8>(Symbol)))
					{
						return false;
					}
					continue;
				}
				if (Symbol == EndOfBlock)
				{
					return true;
				}
				const uint32 LengthCode = Symbol - EndOfBlock - 1;
				if (LengthCode >= LengthBase.size())
				{
					return false;
				}
				const uint32 Length = LengthBase[LengthCode] + Reader.Read(LengthExtra[LengthCode]);
				const uint32 DistanceCode = Distances.Decode(Reader);
				if (DistanceCode >= DistanceBase.size())
				{
					return false;
				}
				const uint32 Distance = DistanceBase[DistanceCode] + Reader.Read(DistanceExtra[DistanceCode]);
				if (!Output.Copy(Distance, Length) || Reader.Overran())
				{
					return false;
				}
			}
		}

		bool ReadDynamicCodes(FLsbBitReader& Reader, FDeflateCode& Literals, FDeflateCode& Distances)
		{
			const uint32 LiteralCount = Reader.Read(5) + 257;
			const uint32 DistanceCount = Reader.Read(5) + 1;
			const uint32 LengthCodeCount = Reader.Read(4) + 4;
			if (LiteralCount > 286 || DistanceCount > 30)
			{
				return false;
			}
			std::array<uint8, CodeLengthOrder.size()> LengthCodeLengths{};
			for (uint32 Index = 0; Index < LengthCodeCount; ++Index)
			{
				LengthCodeLengths[CodeLengthOrder[Index]] = static_cast<uint8>(Reader.Read(3));
			}
			FDeflateCode LengthCode;
			if (!LengthCode.Build(LengthCodeLengths))
			{
				return false;
			}

			// The literal and distance code lengths are one sequence: a run may cross from one to the other.
			std::array<uint8, 286 + 30> Lengths{};
			for (uint32 Index = 0; Index < LiteralCount + DistanceCount;)
			{
				const uint32 Symbol = LengthCode.Decode(Reader);
				if (Symbol < 16)
				{
					Lengths[Index++] = static_cast<uint8>(Symbol);
					continue;
				}
				uint8 Repeated = 0;
				uint32 Count = 0;
				if (Symbol == 16)
				{
					if (Index == 0)
					{
						return false;
					}
					Repeated = Lengths[Index - 1];
					Count = 3 + Reader.Read(2);
				}
				else if (Symbol == 17)
				{
					Count = 3 + Reader.Read(3);
				}
				else if (Symbol == 18)
				{
					Count = 11 + Reader.Read(7);
				}
				else
				{
					return false;
				}
				if (Count > LiteralCount + DistanceCount - Index)
				{
					return false;
				}
				std::fill_n(Lengths.begin() + Index, Count, Repeated);
				Index += Count;
			}
			if (Lengths[EndOfBlock] == 0 || Reader.Overran())
			{
				return false;
			}
			const std::span<const uint8> All(Lengths);
			return Literals.Build(All.first(LiteralCount)) && Distances.Build(All.subspan(LiteralCount, DistanceCount));
		}
	}

	bool FDeflateCode::Build(std::span<const uint8> Lengths)
	{
		Counts.fill(0);
		for (const uint8 Length : Lengths)
		{
			++Counts[Length];
		}
		Counts[0] = 0;
		int32 Left = 1;
		for (uint32 Length = 1; Length <= MaxBits; ++Length)
		{
			Left = Left * 2 - Counts[Length];
			if (Left < 0)
			{
				return false;
			}
		}
		const uint32 Used = static_cast<uint32>(Lengths.size() - static_cast<std::size_t>(std::count(Lengths.begin(), Lengths.end(), uint8{0})));
		if (Left > 0 && !(Used == 1 && Counts[1] == 1) && Used != 0)
		{
			return false;
		}

		std::array<uint16, MaxBits + 2> Offsets{};
		for (uint32 Length = 1; Length <= MaxBits; ++Length)
		{
			Offsets[Length + 1] = static_cast<uint16>(Offsets[Length] + Counts[Length]);
		}
		for (uint32 Symbol = 0; Symbol < Lengths.size(); ++Symbol)
		{
			if (Lengths[Symbol] != 0)
			{
				Symbols[Offsets[Lengths[Symbol]]++] = static_cast<uint16>(Symbol);
			}
		}

		Fast.fill(0);
		uint32 Code = 0;
		uint32 Index = 0;
		for (uint32 Length = 1; Length <= FastBits; ++Length)
		{
			for (uint32 Each = 0; Each < Counts[Length]; ++Each, ++Code, ++Index)
			{
				const uint16 Entry = static_cast<uint16>(Symbols[Index] << 4 | Length);
				for (uint32 Slot = Reverse(Code, Length); Slot < FastSize; Slot += 1u << Length)
				{
					Fast[Slot] = Entry;
				}
			}
			Code <<= 1;
		}
		return true;
	}

	FInflater::FInflater()
	{
		std::array<uint8, LiteralCodes> FixedLiteralLengths{};
		std::fill_n(FixedLiteralLengths.begin(), 144, uint8{8});
		std::fill_n(FixedLiteralLengths.begin() + 144, 112, uint8{9});
		std::fill_n(FixedLiteralLengths.begin() + 256, 24, uint8{7});
		std::fill_n(FixedLiteralLengths.begin() + 280, 8, uint8{8});
		std::array<uint8, DistanceCodes> FixedDistanceLengths{};
		FixedDistanceLengths.fill(5);
		FixedLiterals.Build(FixedLiteralLengths);
		FixedDistances.Build(FixedDistanceLengths);
	}

	bool FInflater::Inflate(std::span<const uint8> Packed, std::span<uint8> Destination)
	{
		FLsbBitReader Reader(Packed);
		FOutput Output(Destination);
		for (bool bFinal = false; !bFinal;)
		{
			bFinal = Reader.Read(1) != 0;
			bool bDone = false;
			switch (Reader.Read(2))
			{
			case 0:
				bDone = Stored(Reader, Output);
				break;
			case 1:
				bDone = Codes(Reader, Output, FixedLiterals, FixedDistances);
				break;
			case 2:
				bDone = ReadDynamicCodes(Reader, Literals, Distances) && Codes(Reader, Output, Literals, Distances);
				break;
			default:
				break;
			}
			if (!bDone || Reader.Overran())
			{
				return false;
			}
		}
		return Output.IsFull();
	}
}
