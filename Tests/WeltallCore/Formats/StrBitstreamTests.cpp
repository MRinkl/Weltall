#include "Formats/StrBitstream.h"
#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

#include <string_view>

using namespace Weltall;
using Testing::FByteWriter;

namespace
{
	/** Writes a movie frame's bitstream: the header, then bits from each halfword's top down. */
	class FBitstreamWriter
	{
	public:
		explicit FBitstreamWriter(uint16 QuantScale, uint16 Version = 2)
		{
			Header.U16(0).U16(0x3800).U16(QuantScale).U16(Version);
		}

		FBitstreamWriter& Bits(std::string_view Text)
		{
			for (const char Bit : Text)
			{
				Pending = static_cast<uint16>(Pending << 1 | (Bit == '1' ? 1 : 0));
				if (++PendingCount == 16)
				{
					Body.U16(Pending);
					PendingCount = 0;
				}
			}
			return *this;
		}

		FBitstreamWriter& Value(uint32 Number, int32 Count)
		{
			for (int32 Bit = Count - 1; Bit >= 0; --Bit)
			{
				Bits((Number >> Bit) & 1 ? "1" : "0");
			}
			return *this;
		}

		std::vector<uint8> Finish()
		{
			while (PendingCount != 0)
			{
				Bits("0");
			}
			std::vector<uint8> Bytes = Header.Bytes;
			Bytes.insert(Bytes.end(), Body.Bytes.begin(), Body.Bytes.end());
			return Bytes;
		}

	private:
		FByteWriter Header;
		FByteWriter Body;
		uint16 Pending = 0;
		int32 PendingCount = 0;
	};

	constexpr uint16 EndOfBlock = 0xFE00;
}

TEST_CASE("A movie bitstream expands into DC codes, coefficients and block ends", "[Formats]")
{
	FBitstreamWriter Writer(3);
	// Block 1: DC 5, run 0 level +1, run 0 level -2, end.
	Writer.Value(5, 10).Bits("110").Bits("01001").Bits("10");
	// Block 2: DC -1, an escape to the literal code (run 3, level 31), end.
	Writer.Value(0x3FF, 10).Bits("000001").Value(3 << 10 | 31, 16).Bits("10");
	// Block 3: DC 0, the longest code (run 27, level +1), end.
	Writer.Value(0, 10).Bits("00000000000111110").Bits("10");
	// Blocks 4-6: DC 7 and nothing else.
	for (int32 Block = 0; Block < 3; ++Block)
	{
		Writer.Value(7, 10).Bits("10");
	}
	const auto Codes = DecodeStrBitstream(Writer.Finish(), 16, 16);
	REQUIRE(Codes);
	const uint16 Scale = 3 << 10;
	const std::vector<uint16> Expected = {
		Scale | 5, 0x0001, 0x03FE, EndOfBlock,
		Scale | 0x3FF, 3 << 10 | 31, EndOfBlock,
		Scale, 27 << 10 | 1, EndOfBlock,
		Scale | 7, EndOfBlock, Scale | 7, EndOfBlock, Scale | 7, EndOfBlock,
	};
	CHECK(*Codes == Expected);
}

TEST_CASE("A movie bitstream is refused when it is not version 2 or ends early", "[Formats]")
{
	CHECK_FALSE(DecodeStrBitstream(FBitstreamWriter(1, 3).Value(1, 10).Bits("10").Finish(), 16, 16));
	// The DC value 1FFh marks the end of the frame's data, here after one of six blocks.
	FBitstreamWriter Short(1);
	Short.Value(1, 10).Bits("10").Value(0x1FF, 10);
	CHECK_FALSE(DecodeStrBitstream(Short.Finish(), 16, 16));
	// Sixteen zero bits are no code.
	FBitstreamWriter Invalid(1);
	Invalid.Value(1, 10).Bits("0000000000000000");
	CHECK_FALSE(DecodeStrBitstream(Invalid.Finish(), 16, 16));
	CHECK_FALSE(DecodeStrBitstream(FBitstreamWriter(1).Value(1, 10).Bits("10").Finish(), 16, 8));
}
