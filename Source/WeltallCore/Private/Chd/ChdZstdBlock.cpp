#include "Chd/ChdZstdBlock.h"

namespace Chd::Zstd
{
	namespace
	{
		constexpr uint32 MostBlockBytes = 128 * 1024;

		/** A literal or match length code's value: a base, and how many bits read after it add to it. */
		struct FLengthCode
		{
			uint32 Base;
			uint8 Bits;
		};

		constexpr std::array<FLengthCode, 36> LiteralLengthCodes{{
			{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}, {6, 0}, {7, 0}, {8, 0}, {9, 0}, {10, 0}, {11, 0},
			{12, 0}, {13, 0}, {14, 0}, {15, 0}, {16, 1}, {18, 1}, {20, 1}, {22, 1}, {24, 2}, {28, 2}, {32, 3}, {40, 3},
			{48, 4}, {64, 6}, {128, 7}, {256, 8}, {512, 9}, {1024, 10}, {2048, 11}, {4096, 12}, {8192, 13}, {16384, 14},
			{32768, 15}, {65536, 16},
		}};

		constexpr std::array<FLengthCode, 53> MatchLengthCodes{{
			{3, 0}, {4, 0}, {5, 0}, {6, 0}, {7, 0}, {8, 0}, {9, 0}, {10, 0}, {11, 0}, {12, 0}, {13, 0}, {14, 0},
			{15, 0}, {16, 0}, {17, 0}, {18, 0}, {19, 0}, {20, 0}, {21, 0}, {22, 0}, {23, 0}, {24, 0}, {25, 0}, {26, 0},
			{27, 0}, {28, 0}, {29, 0}, {30, 0}, {31, 0}, {32, 0}, {33, 0}, {34, 0}, {35, 1}, {37, 1}, {39, 1}, {41, 1},
			{43, 2}, {47, 2}, {51, 3}, {59, 3}, {67, 4}, {83, 4}, {99, 5}, {131, 7}, {259, 8}, {515, 9}, {1027, 10},
			{2051, 11}, {4099, 12}, {8195, 13}, {16387, 14}, {32771, 15}, {65539, 16},
		}};

		/** The three codes a sequence is made of, in the order their tables come. */
		enum ECode : uint32
		{
			LiteralLength,
			Offset,
			MatchLength,
		};

		struct FCodeKind
		{
			uint32 MaxSymbol;
			uint32 MaxAccuracyLog;
			uint32 DefaultAccuracyLog;
		};

		constexpr std::array<FCodeKind, 3> CodeKinds{{{35, 9, 6}, {31, 8, 5}, {52, 9, 6}}};

		/** The tables a block may use without describing them, as normalized counts. */
		constexpr std::array<int16, 36> DefaultLiteralLengths{
			4, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2, 1, 1, 1, 1, 1, -1, -1, -1, -1};
		constexpr std::array<int16, 29> DefaultOffsets{
			1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1};
		constexpr std::array<int16, 53> DefaultMatchLengths{
			1, 4, 3, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1};
	}

	FBlockDecoder::FBlockDecoder()
	{
		Defaults[LiteralLength].Build(DefaultLiteralLengths, CodeKinds[LiteralLength].DefaultAccuracyLog);
		Defaults[Offset].Build(DefaultOffsets, CodeKinds[Offset].DefaultAccuracyLog);
		Defaults[MatchLength].Build(DefaultMatchLengths, CodeKinds[MatchLength].DefaultAccuracyLog);
	}

	void FBlockDecoder::BeginFrame()
	{
		Repeats = {1, 4, 8};
		bHasTable = {};
		bHasHuffman = false;
	}

	bool FBlockDecoder::Decode(std::span<const uint8> Bytes, FFrameOutput& Output)
	{
		const auto LiteralsBytes = ReadLiterals(Bytes);
		return LiteralsBytes && Sequences(Bytes.subspan(*LiteralsBytes), Output);
	}

	std::optional<std::size_t> FBlockDecoder::ReadLiterals(std::span<const uint8> Bytes)
	{
		if (Bytes.empty())
		{
			return std::nullopt;
		}
		const uint32 Type = Bytes[0] & 3;
		const uint32 SizeFormat = Bytes[0] >> 2 & 3;
		if (Type < 2)
		{
			// Plain or one byte repeated: the size in 5, 12 or 20 bits.
			const std::size_t HeaderBytes = SizeFormat == 1 ? 2 : SizeFormat == 3 ? 3 : 1;
			if (Bytes.size() < HeaderBytes)
			{
				return std::nullopt;
			}
			const uint32 Header = static_cast<uint32>(ReadLittle(Bytes, 0, HeaderBytes));
			const std::size_t Count = HeaderBytes == 1 ? Header >> 3 : Header >> 4;
			const std::size_t Stored = Type == 0 ? Count : 1;
			if (Count > MostBlockBytes || Bytes.size() - HeaderBytes < Stored)
			{
				return std::nullopt;
			}
			if (Type == 0)
			{
				Literals.assign(Bytes.begin() + static_cast<std::ptrdiff_t>(HeaderBytes), Bytes.begin() + static_cast<std::ptrdiff_t>(HeaderBytes + Count));
			}
			else
			{
				Literals.assign(Count, Bytes[HeaderBytes]);
			}
			return HeaderBytes + Stored;
		}

		// Huffman-coded, with a new tree or the last one: the sizes in 10, 14 or 18 bits; one stream or four.
		const std::size_t HeaderBytes = SizeFormat < 2 ? 3 : SizeFormat == 2 ? 4 : 5;
		const uint32 SizeBits = SizeFormat < 2 ? 10 : SizeFormat == 2 ? 14 : 18;
		if (Bytes.size() < HeaderBytes)
		{
			return std::nullopt;
		}
		const uint64 Header = ReadLittle(Bytes, 0, HeaderBytes);
		const std::size_t Count = static_cast<std::size_t>(Header >> 4 & ((1u << SizeBits) - 1));
		const std::size_t PackedBytes = static_cast<std::size_t>(Header >> (4 + SizeBits));
		if (Count > MostBlockBytes || Bytes.size() - HeaderBytes < PackedBytes)
		{
			return std::nullopt;
		}
		std::span<const uint8> Packed = Bytes.subspan(HeaderBytes, PackedBytes);
		if (Type == 2)
		{
			const auto TreeBytes = ReadHuffmanTable(Packed, Huffman);
			if (!TreeBytes)
			{
				return std::nullopt;
			}
			bHasHuffman = true;
			Packed = Packed.subspan(*TreeBytes);
		}
		if (!bHasHuffman || !DecodeStreams(Packed, Count, SizeFormat != 0))
		{
			return std::nullopt;
		}
		return HeaderBytes + PackedBytes;
	}

	bool FBlockDecoder::DecodeStreams(std::span<const uint8> Bytes, std::size_t Count, bool bFourStreams)
	{
		Literals.resize(Count);
		const auto DecodeStream = [this](std::span<const uint8> Stream, std::size_t First, std::size_t StreamCount) {
			FBackwardBits Bits;
			if (!Bits.Begin(Stream))
			{
				return false;
			}
			for (std::size_t Index = First; Index < First + StreamCount; ++Index)
			{
				Literals[Index] = Huffman.Decode(Bits);
			}
			return Bits.IsDone();
		};
		if (!bFourStreams)
		{
			return DecodeStream(Bytes, 0, Count);
		}
		// A jump table gives the first three streams' sizes; each stream decodes a quarter of the literals, rounded up.
		constexpr std::size_t JumpTableBytes = 6;
		const std::size_t Segment = (Count + 3) / 4;
		if (Bytes.size() < JumpTableBytes || 3 * Segment > Count)
		{
			return false;
		}
		std::array<std::size_t, 4> Sizes{};
		std::size_t Total = JumpTableBytes;
		for (std::size_t Stream = 0; Stream < 3; ++Stream)
		{
			Sizes[Stream] = static_cast<std::size_t>(ReadLittle(Bytes, Stream * 2, 2));
			Total += Sizes[Stream];
		}
		if (Total > Bytes.size())
		{
			return false;
		}
		Sizes[3] = Bytes.size() - Total;
		std::size_t At = JumpTableBytes;
		for (std::size_t Stream = 0; Stream < 4; ++Stream)
		{
			const std::size_t StreamCount = Stream < 3 ? Segment : Count - 3 * Segment;
			if (!DecodeStream(Bytes.subspan(At, Sizes[Stream]), Stream * Segment, StreamCount))
			{
				return false;
			}
			At += Sizes[Stream];
		}
		return true;
	}

	bool FBlockDecoder::Sequences(std::span<const uint8> Bytes, FFrameOutput& Output)
	{
		if (Bytes.empty())
		{
			return false;
		}
		// The number of sequences in one byte below 128, two below 255, else three.
		const uint32 First = Bytes[0];
		if (First == 0)
		{
			return Bytes.size() == 1 && Output.Put(Literals);
		}
		std::size_t At = First < 128 ? 1 : First < 255 ? 2 : 3;
		if (Bytes.size() < At)
		{
			return false;
		}
		const std::size_t Count = First < 128 ? First : First < 255 ? ((First - 128) << 8) + Bytes[1] : ReadLittle(Bytes, 1, 2) + 0x7F00;
		if (Bytes.size() <= At)
		{
			return false;
		}
		const uint32 Modes = Bytes[At++];
		if ((Modes & 3) != 0)
		{
			return false;
		}
		for (uint32 Code = LiteralLength; Code <= MatchLength; ++Code)
		{
			const uint32 Mode = Modes >> (6 - 2 * Code) & 3;
			if (Mode == 0)
			{
				Tables[Code] = Defaults[Code];
			}
			else if (Mode == 1)
			{
				if (At >= Bytes.size() || Bytes[At] > CodeKinds[Code].MaxSymbol)
				{
					return false;
				}
				Tables[Code].BuildRle(Bytes[At++]);
			}
			else if (Mode == 2)
			{
				const auto TableBytes = ReadFseTable(Bytes.subspan(At), CodeKinds[Code].MaxSymbol, CodeKinds[Code].MaxAccuracyLog, Tables[Code]);
				if (!TableBytes)
				{
					return false;
				}
				At += *TableBytes;
			}
			else if (!bHasTable[Code])
			{
				return false;
			}
			bHasTable[Code] = true;
		}

		FBackwardBits Bits;
		if (!Bits.Begin(Bytes.subspan(At)))
		{
			return false;
		}
		std::array<FFseState, 3> States;
		for (uint32 Code = LiteralLength; Code <= MatchLength; ++Code)
		{
			States[Code].Begin(Tables[Code], Bits);
		}
		std::size_t LiteralAt = 0;
		for (std::size_t Sequence = 0; Sequence < Count; ++Sequence)
		{
			const uint32 OffsetCode = States[Offset].Symbol();
			const uint32 MatchCode = States[MatchLength].Symbol();
			const uint32 LiteralCode = States[LiteralLength].Symbol();
			if (OffsetCode > 31)
			{
				return false;
			}
			const uint32 OffsetValue = (1u << OffsetCode) + Bits.Read(OffsetCode);
			const uint32 MatchBytes = MatchLengthCodes[MatchCode].Base + Bits.Read(MatchLengthCodes[MatchCode].Bits);
			const uint32 LiteralBytes = LiteralLengthCodes[LiteralCode].Base + Bits.Read(LiteralLengthCodes[LiteralCode].Bits);

			// Values 1 to 3 repeat a recent offset, which one shifted by one when the sequence has no literals.
			uint32 Distance = 0;
			if (OffsetValue > 3)
			{
				Distance = OffsetValue - 3;
				Repeats = {Distance, Repeats[0], Repeats[1]};
			}
			else
			{
				const uint32 Index = OffsetValue - 1 + (LiteralBytes == 0 ? 1 : 0);
				Distance = Index == 3 ? Repeats[0] - 1 : Repeats[Index];
				if (Index != 0)
				{
					if (Index != 1)
					{
						Repeats[2] = Repeats[1];
					}
					Repeats[1] = Repeats[0];
					Repeats[0] = Distance;
				}
			}
			if (Sequence + 1 < Count)
			{
				States[LiteralLength].Update(Bits);
				States[MatchLength].Update(Bits);
				States[Offset].Update(Bits);
			}
			if (LiteralBytes > Literals.size() - LiteralAt || !Output.Put(std::span(Literals).subspan(LiteralAt, LiteralBytes))
				|| !Output.Copy(Distance, MatchBytes))
			{
				return false;
			}
			LiteralAt += LiteralBytes;
		}
		return Bits.IsDone() && Output.Put(std::span(Literals).subspan(LiteralAt));
	}
}
