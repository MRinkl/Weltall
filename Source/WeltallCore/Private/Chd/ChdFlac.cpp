#include "Chd/ChdFlac.h"

#include <algorithm>
#include <bit>
#include <limits>

namespace Chd
{
	namespace
	{
		constexpr uint32 Stereo = 2;
		constexpr uint32 SampleBits = 16;
		constexpr uint32 MostBlockSize = 65535;
		constexpr uint32 MostLpcOrder = 32;

		/** How the two channels of a frame are coded. */
		enum EChannels : uint32
		{
			IndependentStereo = 1,
			LeftSide = 8,
			SideRight = 9,
			MidSide = 10,
		};

		/** Bits read most significant first, with the byte position a frame's checksums need. */
		class FBits
		{
		public:
			explicit FBits(std::span<const uint8> InBytes) : Bytes(InBytes) {}

			/** Count bits (at most 32), zeros past the end. */
			uint32 Read(uint32 Count)
			{
				if (Count == 0)
				{
					return 0;
				}
				Fill(Count);
				const uint32 Value = static_cast<uint32>(Buffer >> (64 - Count));
				Buffer <<= Count;
				Buffered -= Count;
				return Value;
			}

			int32 ReadSigned(uint32 Count)
			{
				if (Count == 0)
				{
					return 0;
				}
				const uint32 Value = Read(Count);
				return static_cast<int32>(Value << (32 - Count)) >> (32 - Count);
			}

			/** The zeros before the next 1, which is taken too; at most Limit of them. */
			uint32 ReadUnary(uint32 Limit)
			{
				uint32 Zeros = 0;
				for (;;)
				{
					Fill(32);
					const uint32 Top = static_cast<uint32>(Buffer >> 32);
					if (Top != 0)
					{
						const uint32 Leading = static_cast<uint32>(std::countl_zero(Top));
						Zeros += Leading;
						Buffer <<= Leading + 1;
						Buffered -= Leading + 1;
						return Zeros;
					}
					Zeros += 32;
					Buffer <<= 32;
					Buffered -= 32;
					if (Zeros > Limit || Overran())
					{
						return Zeros;
					}
				}
			}

			void AlignToByte()
			{
				const uint32 Drop = Buffered % 8;
				Buffer <<= Drop;
				Buffered -= Drop;
			}

			/** The bytes taken so far, when the position is on a byte boundary. */
			std::size_t BytePosition() const { return BitPosition() / 8; }

			bool Overran() const { return BitPosition() > Bytes.size() * 8; }

		private:
			std::size_t BitPosition() const { return NextByte * 8 - Buffered; }

			void Fill(uint32 Count)
			{
				while (Buffered < Count)
				{
					const uint64 Byte = NextByte < Bytes.size() ? Bytes[NextByte] : 0;
					Buffer |= Byte << (56 - Buffered);
					++NextByte;
					Buffered += 8;
				}
			}

			std::span<const uint8> Bytes;
			std::size_t NextByte = 0;
			uint64 Buffer = 0;
			uint32 Buffered = 0;
		};

		uint8 Crc8(std::span<const uint8> Bytes)
		{
			uint32 Crc = 0;
			for (const uint8 Byte : Bytes)
			{
				Crc ^= Byte;
				for (int Bit = 0; Bit < 8; ++Bit)
				{
					Crc = (Crc & 0x80) != 0 ? (Crc << 1 ^ 0x07) & 0xFF : Crc << 1 & 0xFF;
				}
			}
			return static_cast<uint8>(Crc);
		}

		uint16 Crc16(std::span<const uint8> Bytes)
		{
			uint32 Crc = 0;
			for (const uint8 Byte : Bytes)
			{
				Crc ^= static_cast<uint32>(Byte) << 8;
				for (int Bit = 0; Bit < 8; ++Bit)
				{
					Crc = (Crc & 0x8000) != 0 ? (Crc << 1 ^ 0x8005) & 0xFFFF : Crc << 1 & 0xFFFF;
				}
			}
			return static_cast<uint16>(Crc);
		}

		struct FFrameHeader
		{
			uint32 BlockSize = 0;
			uint32 Channels = 0;
		};

		/** Reads a frame's header and checks its CRC-8; false when it is not the header of a 16-bit stereo frame. */
		bool ReadFrameHeader(FBits& Bits, std::span<const uint8> Frame, FFrameHeader& Header)
		{
			// 14 sync bits and a reserved 0, then the blocking strategy, which does not matter here.
			if (Bits.Read(15) != 0x7FFC)
			{
				return false;
			}
			Bits.Read(1);
			const uint32 BlockSizeCode = Bits.Read(4);
			const uint32 RateCode = Bits.Read(4);
			Header.Channels = Bits.Read(4);
			const uint32 SizeCode = Bits.Read(3);
			if (Bits.Read(1) != 0 || RateCode == 15 || BlockSizeCode == 0 || (SizeCode != 0 && SizeCode != 4))
			{
				return false;
			}
			if (Header.Channels != IndependentStereo && Header.Channels != LeftSide && Header.Channels != SideRight && Header.Channels != MidSide)
			{
				return false;
			}
			// The frame or sample number, coded as UTF-8 codes a character, up to seven bytes long.
			const uint32 First = Bits.Read(8);
			const uint32 Following = First < 0x80 ? 0 : static_cast<uint32>(std::countl_one(static_cast<uint8>(First))) - 1;
			if ((First >= 0x80 && First < 0xC0) || First == 0xFF)
			{
				return false;
			}
			for (uint32 Index = 0; Index < Following; ++Index)
			{
				if ((Bits.Read(8) & 0xC0) != 0x80)
				{
					return false;
				}
			}
			if (BlockSizeCode == 1)
			{
				Header.BlockSize = 192;
			}
			else if (BlockSizeCode <= 5)
			{
				Header.BlockSize = 576u << (BlockSizeCode - 2);
			}
			else if (BlockSizeCode == 6)
			{
				Header.BlockSize = Bits.Read(8) + 1;
			}
			else if (BlockSizeCode == 7)
			{
				Header.BlockSize = Bits.Read(16) + 1;
			}
			else
			{
				Header.BlockSize = 256u << (BlockSizeCode - 8);
			}
			if (RateCode == 12)
			{
				Bits.Read(8);
			}
			else if (RateCode == 13 || RateCode == 14)
			{
				Bits.Read(16);
			}
			const std::size_t HeaderBytes = Bits.BytePosition();
			return Header.BlockSize <= MostBlockSize && !Bits.Overran() && Bits.Read(8) == Crc8(Frame.first(HeaderBytes));
		}

		/** Decodes a residual, partitioned and Rice-coded, into the samples after the predictor's first Order. */
		bool ReadResidual(FBits& Bits, std::span<int32> Samples, uint32 Order)
		{
			const uint32 Method = Bits.Read(2);
			if (Method > 1)
			{
				return false;
			}
			const uint32 ParameterBits = Method == 0 ? 4 : 5;
			const uint32 Escape = (1u << ParameterBits) - 1;
			const uint32 PartitionOrder = Bits.Read(4);
			const uint32 BlockSize = static_cast<uint32>(Samples.size());
			const uint32 PartitionSize = BlockSize >> PartitionOrder;
			if ((PartitionSize << PartitionOrder) != BlockSize || PartitionSize < Order)
			{
				return false;
			}
			uint32 Index = Order;
			for (uint32 Partition = 0; Partition < (1u << PartitionOrder); ++Partition)
			{
				const uint32 End = (Partition + 1) * PartitionSize;
				const uint32 Parameter = Bits.Read(ParameterBits);
				if (Parameter == Escape)
				{
					const uint32 RawBits = Bits.Read(5);
					for (; Index < End; ++Index)
					{
						Samples[Index] = Bits.ReadSigned(RawBits);
					}
					continue;
				}
				for (; Index < End; ++Index)
				{
					// A quotient past this would not fit the folded value in 32 bits.
					const uint32 MostQuotient = std::numeric_limits<uint32>::max() >> Parameter;
					const uint32 Quotient = Bits.ReadUnary(MostQuotient);
					if (Quotient > MostQuotient)
					{
						return false;
					}
					const uint32 Folded = Quotient << Parameter | Bits.Read(Parameter);
					Samples[Index] = static_cast<int32>(Folded >> 1) ^ -static_cast<int32>(Folded & 1);
				}
				if (Bits.Overran())
				{
					return false;
				}
			}
			return true;
		}

		/** Adds each sample's prediction from the ones before it to its residual. */
		bool Predict(std::span<int32> Samples, std::span<const int32> Coefficients, uint32 Shift)
		{
			const std::size_t Order = Coefficients.size();
			for (std::size_t Index = Order; Index < Samples.size(); ++Index)
			{
				int64 Sum = 0;
				for (std::size_t Tap = 0; Tap < Order; ++Tap)
				{
					Sum += static_cast<int64>(Coefficients[Tap]) * Samples[Index - 1 - Tap];
				}
				const int64 Sample = Samples[Index] + (Sum >> Shift);
				if (Sample < std::numeric_limits<int32>::min() || Sample > std::numeric_limits<int32>::max())
				{
					return false;
				}
				Samples[Index] = static_cast<int32>(Sample);
			}
			return true;
		}

		/** Decodes one channel's subframe of Bps-bit samples. */
		bool ReadSubframe(FBits& Bits, std::span<int32> Samples, uint32 Bps)
		{
			if (Bits.Read(1) != 0)
			{
				return false;
			}
			const uint32 Type = Bits.Read(6);
			uint32 Wasted = 0;
			if (Bits.Read(1) != 0)
			{
				Wasted = Bits.ReadUnary(Bps) + 1;
				if (Wasted >= Bps)
				{
					return false;
				}
				Bps -= Wasted;
			}

			if (Type == 0)
			{
				std::fill(Samples.begin(), Samples.end(), Bits.ReadSigned(Bps));
			}
			else if (Type == 1)
			{
				for (int32& Sample : Samples)
				{
					Sample = Bits.ReadSigned(Bps);
				}
			}
			else if (Type >= 8 && Type <= 12)
			{
				// The fixed predictors of orders 0 to 4: differences of the samples before.
				static constexpr std::array<std::array<int32, 4>, 5> Fixed{{{}, {1}, {2, -1}, {3, -3, 1}, {4, -6, 4, -1}}};
				const uint32 Order = Type - 8;
				if (Order > Samples.size())
				{
					return false;
				}
				for (uint32 Index = 0; Index < Order; ++Index)
				{
					Samples[Index] = Bits.ReadSigned(Bps);
				}
				if (!ReadResidual(Bits, Samples, Order) || !Predict(Samples, std::span(Fixed[Order]).first(Order), 0))
				{
					return false;
				}
			}
			else if (Type >= 32)
			{
				const uint32 Order = Type - 31;
				if (Order > Samples.size())
				{
					return false;
				}
				for (uint32 Index = 0; Index < Order; ++Index)
				{
					Samples[Index] = Bits.ReadSigned(Bps);
				}
				const uint32 Precision = Bits.Read(4) + 1;
				const int32 Shift = Bits.ReadSigned(5);
				if (Precision == 16 || Shift < 0)
				{
					return false;
				}
				std::array<int32, MostLpcOrder> Coefficients{};
				for (uint32 Index = 0; Index < Order; ++Index)
				{
					Coefficients[Index] = Bits.ReadSigned(Precision);
				}
				if (!ReadResidual(Bits, Samples, Order) || !Predict(Samples, std::span(Coefficients).first(Order), static_cast<uint32>(Shift)))
				{
					return false;
				}
			}
			else
			{
				return false;
			}
			if (Wasted != 0)
			{
				for (int32& Sample : Samples)
				{
					Sample = static_cast<int32>(static_cast<uint32>(Sample) << Wasted);
				}
			}
			return !Bits.Overran();
		}
	}

	std::optional<std::size_t> FFlacDecoder::DecodeStereo16(std::span<const uint8> Packed, std::span<uint8> Destination, bool bBigEndian)
	{
		const std::size_t Wanted = Destination.size() / 4;
		std::size_t Done = 0;
		std::size_t FrameStart = 0;
		while (Done < Wanted)
		{
			const std::span<const uint8> Frame = Packed.subspan(FrameStart);
			FBits Bits(Frame);
			FFrameHeader Header;
			if (!ReadFrameHeader(Bits, Frame, Header))
			{
				return std::nullopt;
			}
			for (uint32 Channel = 0; Channel < Stereo; ++Channel)
			{
				Channels[Channel].resize(Header.BlockSize);
				// The side channel's samples are differences: they take a bit more.
				const bool bSide = (Header.Channels == LeftSide && Channel == 1) || (Header.Channels == SideRight && Channel == 0)
					|| (Header.Channels == MidSide && Channel == 1);
				if (!ReadSubframe(Bits, Channels[Channel], SampleBits + (bSide ? 1 : 0)))
				{
					return std::nullopt;
				}
			}
			Bits.AlignToByte();
			const std::size_t FrameBytes = Bits.BytePosition();
			const uint32 Crc = Bits.Read(16);
			if (Bits.Overran() || Crc != Crc16(Frame.first(FrameBytes)))
			{
				return std::nullopt;
			}
			FrameStart += FrameBytes + 2;

			const std::size_t Taken = std::min<std::size_t>(Header.BlockSize, Wanted - Done);
			for (std::size_t Index = 0; Index < Taken; ++Index)
			{
				int32 Left = Channels[0][Index];
				int32 Right = Channels[1][Index];
				switch (Header.Channels)
				{
				case LeftSide:
					Right = Left - Right;
					break;
				case SideRight:
					Left = Left + Right;
					break;
				case MidSide:
				{
					const int32 Mid = static_cast<int32>(static_cast<uint32>(Left) << 1) | (Right & 1);
					Left = (Mid + Right) >> 1;
					Right = (Mid - Right) >> 1;
					break;
				}
				default:
					break;
				}
				if (Left < std::numeric_limits<int16>::min() || Left > std::numeric_limits<int16>::max()
					|| Right < std::numeric_limits<int16>::min() || Right > std::numeric_limits<int16>::max())
				{
					return std::nullopt;
				}
				uint8* Out = Destination.data() + (Done + Index) * 4;
				for (const int32 Sample : {Left, Right})
				{
					const uint16 Bits16 = static_cast<uint16>(Sample);
					Out[bBigEndian ? 0 : 1] = static_cast<uint8>(Bits16 >> 8);
					Out[bBigEndian ? 1 : 0] = static_cast<uint8>(Bits16);
					Out += 2;
				}
			}
			Done += Taken;
		}
		return FrameStart;
	}
}
