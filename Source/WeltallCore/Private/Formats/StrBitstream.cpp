#include "Formats/StrBitstream.h"

#include "Foundation/LittleEndian.h"

#include <array>
#include <string>

namespace Weltall
{
	namespace
	{
		constexpr std::size_t HeaderBytes = 8;
		constexpr uint16 HeaderMagic = 0x3800;
		constexpr uint16 SupportedVersion = 2;
		constexpr int32 DcBits = 10;
		/** A DC value of 1FFh marks the end of the frame's data. */
		constexpr uint32 EndOfFrameDc = 0x1FF;
		constexpr uint16 EndOfBlockCode = 0xFE00;
		constexpr int32 EscapeCodeBits = 16;

		/** A coefficient code without its sign bit, which follows it (0 positive, 1 negative). */
		struct FCoefficientCode
		{
			uint32 Bits;
			uint8 Length;
			uint8 Run;
			uint8 Level;
		};

		/** The MPEG-1 coefficient codes, as Sony's library decodes them. */
		constexpr FCoefficientCode CoefficientCodes[] = {
			{0b11, 2, 0, 1}, {0b011, 3, 1, 1}, {0b0100, 4, 0, 2}, {0b0101, 4, 2, 1},
			{0b00101, 5, 0, 3}, {0b00110, 5, 4, 1}, {0b00111, 5, 3, 1}, {0b000100, 6, 7, 1},
			{0b000101, 6, 6, 1}, {0b000110, 6, 1, 2}, {0b000111, 6, 5, 1}, {0b0000100, 7, 2, 2},
			{0b0000101, 7, 9, 1}, {0b0000110, 7, 0, 4}, {0b0000111, 7, 8, 1}, {0b00100000, 8, 13, 1},
			{0b00100001, 8, 0, 6}, {0b00100010, 8, 12, 1}, {0b00100011, 8, 11, 1}, {0b00100100, 8, 3, 2},
			{0b00100101, 8, 1, 3}, {0b00100110, 8, 0, 5}, {0b00100111, 8, 10, 1}, {0b0000001000, 10, 16, 1},
			{0b0000001001, 10, 5, 2}, {0b0000001010, 10, 0, 7}, {0b0000001011, 10, 2, 3}, {0b0000001100, 10, 1, 4},
			{0b0000001101, 10, 15, 1}, {0b0000001110, 10, 14, 1}, {0b0000001111, 10, 4, 2}, {0b000000010000, 12, 0, 11},
			{0b000000010001, 12, 8, 2}, {0b000000010010, 12, 4, 3}, {0b000000010011, 12, 0, 10}, {0b000000010100, 12, 2, 4},
			{0b000000010101, 12, 7, 2}, {0b000000010110, 12, 21, 1}, {0b000000010111, 12, 20, 1}, {0b000000011000, 12, 0, 9},
			{0b000000011001, 12, 19, 1}, {0b000000011010, 12, 18, 1}, {0b000000011011, 12, 1, 5}, {0b000000011100, 12, 3, 3},
			{0b000000011101, 12, 0, 8}, {0b000000011110, 12, 6, 2}, {0b000000011111, 12, 17, 1}, {0b0000000010000, 13, 10, 2},
			{0b0000000010001, 13, 9, 2}, {0b0000000010010, 13, 5, 3}, {0b0000000010011, 13, 3, 4}, {0b0000000010100, 13, 2, 5},
			{0b0000000010101, 13, 1, 7}, {0b0000000010110, 13, 1, 6}, {0b0000000010111, 13, 0, 15}, {0b0000000011000, 13, 0, 14},
			{0b0000000011001, 13, 0, 13}, {0b0000000011010, 13, 0, 12}, {0b0000000011011, 13, 26, 1}, {0b0000000011100, 13, 25, 1},
			{0b0000000011101, 13, 24, 1}, {0b0000000011110, 13, 23, 1}, {0b0000000011111, 13, 22, 1}, {0b00000000010000, 14, 0, 31},
			{0b00000000010001, 14, 0, 30}, {0b00000000010010, 14, 0, 29}, {0b00000000010011, 14, 0, 28}, {0b00000000010100, 14, 0, 27},
			{0b00000000010101, 14, 0, 26}, {0b00000000010110, 14, 0, 25}, {0b00000000010111, 14, 0, 24}, {0b00000000011000, 14, 0, 23},
			{0b00000000011001, 14, 0, 22}, {0b00000000011010, 14, 0, 21}, {0b00000000011011, 14, 0, 20}, {0b00000000011100, 14, 0, 19},
			{0b00000000011101, 14, 0, 18}, {0b00000000011110, 14, 0, 17}, {0b00000000011111, 14, 0, 16}, {0b000000000010000, 15, 0, 40},
			{0b000000000010001, 15, 0, 39}, {0b000000000010010, 15, 0, 38}, {0b000000000010011, 15, 0, 37}, {0b000000000010100, 15, 0, 36},
			{0b000000000010101, 15, 0, 35}, {0b000000000010110, 15, 0, 34}, {0b000000000010111, 15, 0, 33}, {0b000000000011000, 15, 0, 32},
			{0b000000000011001, 15, 1, 14}, {0b000000000011010, 15, 1, 13}, {0b000000000011011, 15, 1, 12}, {0b000000000011100, 15, 1, 11},
			{0b000000000011101, 15, 1, 10}, {0b000000000011110, 15, 1, 9}, {0b000000000011111, 15, 1, 8}, {0b0000000000010000, 16, 1, 18},
			{0b0000000000010001, 16, 1, 17}, {0b0000000000010010, 16, 1, 16}, {0b0000000000010011, 16, 1, 15}, {0b0000000000010100, 16, 6, 3},
			{0b0000000000010101, 16, 16, 2}, {0b0000000000010110, 16, 15, 2}, {0b0000000000010111, 16, 14, 2}, {0b0000000000011000, 16, 13, 2},
			{0b0000000000011001, 16, 12, 2}, {0b0000000000011010, 16, 11, 2}, {0b0000000000011011, 16, 31, 1}, {0b0000000000011100, 16, 30, 1},
			{0b0000000000011101, 16, 29, 1}, {0b0000000000011110, 16, 28, 1}, {0b0000000000011111, 16, 27, 1},
		};
		constexpr uint32 EndOfBlockBits = 0b10;
		constexpr uint8 EndOfBlockLength = 2;
		constexpr uint32 EscapeBits = 0b000001;
		constexpr uint8 EscapeLength = 6;

		enum class ECodeKind : uint8
		{
			Invalid,
			Coefficient,
			EndOfBlock,
			Escape,
		};

		struct FTableEntry
		{
			ECodeKind Kind = ECodeKind::Invalid;
			uint8 Length = 0;
			uint16 Code = 0;
		};

		/**
		 * Two lookup levels: the next 13 bits resolve every code up to 13 bits (sign included); the
		 * longer codes all begin with 8 zero bits, and the 9 bits after those resolve them.
		 */
		constexpr int32 FirstLevelBits = 13;
		constexpr int32 LongPrefixBits = 8;
		constexpr int32 SecondLevelBits = 9;

		struct FCodeTables
		{
			std::array<FTableEntry, 1 << FirstLevelBits> First{};
			std::array<FTableEntry, 1 << SecondLevelBits> Second{};

			FCodeTables()
			{
				Add(EndOfBlockBits, EndOfBlockLength, {ECodeKind::EndOfBlock, EndOfBlockLength, EndOfBlockCode});
				Add(EscapeBits, EscapeLength, {ECodeKind::Escape, EscapeLength, 0});
				for (const FCoefficientCode& Code : CoefficientCodes)
				{
					const uint8 Length = static_cast<uint8>(Code.Length + 1);
					const uint16 Run = static_cast<uint16>(Code.Run << 10);
					Add(Code.Bits << 1, Length, {ECodeKind::Coefficient, Length, static_cast<uint16>(Run | Code.Level)});
					Add(Code.Bits << 1 | 1, Length,
						{ECodeKind::Coefficient, Length, static_cast<uint16>(Run | ((0x400 - Code.Level) & 0x3FF))});
				}
			}

			/** Fills every entry whose index begins with the code's bits. */
			void Add(uint32 Bits, uint8 Length, FTableEntry Entry)
			{
				if (Length <= FirstLevelBits)
				{
					Fill(First, Bits << (FirstLevelBits - Length), 1u << (FirstLevelBits - Length), Entry);
					return;
				}
				// A long code: its bits after the 8-zero prefix index the second level, and its entry
				// gives the length that remains once the prefix is skipped.
				const int32 Rest = Length - LongPrefixBits;
				Entry.Length = static_cast<uint8>(Rest);
				Fill(Second, Bits << (SecondLevelBits - Rest), 1u << (SecondLevelBits - Rest), Entry);
			}

			template <std::size_t N>
			static void Fill(std::array<FTableEntry, N>& Table, uint32 First, uint32 Count, FTableEntry Entry)
			{
				for (uint32 Index = First; Index < First + Count; ++Index)
				{
					Table[Index] = Entry;
				}
			}
		};

		const FCodeTables& CodeTables()
		{
			static const FCodeTables Tables;
			return Tables;
		}

		/** Reads bits from the top of little-endian halfwords down; past the data it reads zeros. */
		class FHalfwordBitReader
		{
		public:
			explicit FHalfwordBitReader(std::span<const uint8> InBytes) : Bytes(InBytes) {}

			uint32 Peek(int32 Count)
			{
				Refill();
				return static_cast<uint32>(Window >> (Available - Count)) & ((1u << Count) - 1);
			}

			void Skip(int32 Count)
			{
				Available -= Count;
				Consumed += static_cast<uint64>(Count);
			}

			uint32 Take(int32 Count)
			{
				const uint32 Value = Peek(Count);
				Skip(Count);
				return Value;
			}

			bool Overran() const { return Consumed > Bytes.size() * 8; }

		private:
			void Refill()
			{
				while (Available <= 48)
				{
					const uint16 Halfword = NextByte + 1 < Bytes.size() ? LittleEndian::ReadU16(Bytes, NextByte) : 0;
					Window = Window << 16 | Halfword;
					Available += 16;
					NextByte += 2;
				}
			}

			std::span<const uint8> Bytes;
			std::size_t NextByte = 0;
			uint64 Window = 0;
			int32 Available = 0;
			uint64 Consumed = 0;
		};
	}

	TResult<std::vector<uint16>> DecodeStrBitstream(std::span<const uint8> Frame, int32 Width, int32 Height)
	{
		if (Frame.size() < HeaderBytes || LittleEndian::ReadU16(Frame, 2) != HeaderMagic)
		{
			return MakeError("not a movie frame bitstream");
		}
		const uint16 Version = LittleEndian::ReadU16(Frame, 6);
		if (Version != SupportedVersion)
		{
			return MakeError("unsupported movie bitstream version " + std::to_string(Version));
		}
		if (Width <= 0 || Height <= 0 || Width % 16 != 0 || Height % 16 != 0)
		{
			return MakeError("a movie picture is made of whole 16x16 macroblocks");
		}
		const uint16 DcScale = static_cast<uint16>((LittleEndian::ReadU16(Frame, 4) & 0x3F) << 10);
		const int32 Blocks = (Width / 16) * (Height / 16) * 6;
		const FCodeTables& Tables = CodeTables();
		FHalfwordBitReader Reader(Frame.subspan(HeaderBytes));
		std::vector<uint16> Codes;
		Codes.reserve(static_cast<std::size_t>(Blocks) * 8);
		for (int32 Block = 0; Block < Blocks; ++Block)
		{
			const uint32 Dc = Reader.Take(DcBits);
			if (Dc == EndOfFrameDc)
			{
				return MakeError("the frame's data ends after " + std::to_string(Block) + " blocks");
			}
			Codes.push_back(static_cast<uint16>(DcScale | Dc));
			for (;;)
			{
				const uint32 Next = Reader.Peek(FirstLevelBits);
				FTableEntry Entry = Tables.First[Next];
				if (Entry.Kind == ECodeKind::Invalid && Next >> (FirstLevelBits - LongPrefixBits) == 0)
				{
					Reader.Skip(LongPrefixBits);
					Entry = Tables.Second[Reader.Peek(SecondLevelBits)];
				}
				if (Entry.Kind == ECodeKind::Invalid)
				{
					return MakeError("invalid coefficient code in block " + std::to_string(Block));
				}
				Reader.Skip(Entry.Length);
				Codes.push_back(Entry.Kind == ECodeKind::Escape ? static_cast<uint16>(Reader.Take(EscapeCodeBits)) : Entry.Code);
				if (Entry.Kind == ECodeKind::EndOfBlock)
				{
					break;
				}
			}
			if (Reader.Overran())
			{
				return MakeError("the frame's data ends inside block " + std::to_string(Block));
			}
		}
		return Codes;
	}
}
