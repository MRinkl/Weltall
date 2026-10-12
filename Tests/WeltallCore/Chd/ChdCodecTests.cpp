#include "Chd/ChdFlac.h"
#include "Chd/ChdInflate.h"
#include "Chd/ChdLzma.h"
#include "Chd/ChdZstd.h"
#include "ChdCodecVectors.h"
#include "ChdFixtures.h"

#include <catch_amalgamated.hpp>

#include <vector>

using namespace Chd;
using namespace Chd::Testing;

namespace
{
	/** Bits written least significant first, as Deflate keeps them; a Huffman code goes in from its first bit. */
	class FDeflateWriter
	{
	public:
		FDeflateWriter& Bits(uint32 Value, uint32 Count)
		{
			for (uint32 Bit = 0; Bit < Count; ++Bit)
			{
				Put(Value >> Bit & 1);
			}
			return *this;
		}

		FDeflateWriter& Code(uint32 Value, uint32 Length)
		{
			for (uint32 Bit = Length; Bit > 0; --Bit)
			{
				Put(Value >> (Bit - 1) & 1);
			}
			return *this;
		}

		std::vector<uint8> Bytes() const { return Written; }

	private:
		void Put(uint32 Bit)
		{
			if (Used % 8 == 0)
			{
				Written.push_back(0);
			}
			Written.back() = static_cast<uint8>(Written.back() | Bit << (Used % 8));
			++Used;
		}

		std::vector<uint8> Written;
		uint32 Used = 0;
	};

	/** A final block of the fixed code: a literal, then a match of Length bytes at Distance (codes 257 and 1 or 0), then its end. */
	std::vector<uint8> FixedBlock(uint32 Distance)
	{
		return FDeflateWriter()
			.Bits(1, 1)
			.Bits(1, 2)
			.Code(0x30 + 'a', 8)
			.Code(1, 7)
			.Code(Distance - 1, 5)
			.Code(0, 7)
			.Bytes();
	}
}

TEST_CASE("Deflate streams by zlib unpack to their data", "[Chd]")
{
	FInflater Inflater;
	for (const FVector& Vector : DeflateVectors())
	{
		INFO(Vector.Name);
		const std::vector<uint8> Packed = FromHex(Vector.Hex);
		std::vector<uint8> Output(static_cast<std::size_t>(Vector.OutputBytes));
		REQUIRE(Inflater.Inflate(Packed, Output));
		CHECK(Fnv1a(Output) == Vector.OutputHash);

		// The stream must fill the output exactly.
		std::vector<uint8> Larger(Output.size() + 1);
		CHECK_FALSE(Inflater.Inflate(Packed, Larger));
		if (!Output.empty())
		{
			CHECK_FALSE(Inflater.Inflate(Packed, std::span(Output).first(Output.size() - 1)));
			CHECK_FALSE(Inflater.Inflate(std::span(Packed).first(Packed.size() / 2), Output));
		}
	}
}

TEST_CASE("A Deflate stream that breaks its rules is refused", "[Chd]")
{
	FInflater Inflater;
	std::vector<uint8> Output(4);
	REQUIRE(Inflater.Inflate(FixedBlock(1), Output));
	CHECK(Output == std::vector<uint8>{'a', 'a', 'a', 'a'});

	// A match reaching back past the first byte.
	CHECK_FALSE(Inflater.Inflate(FixedBlock(2), Output));
	// Block type 3 does not exist.
	CHECK_FALSE(Inflater.Inflate(FDeflateWriter().Bits(1, 1).Bits(3, 2).Bytes(), std::span(Output).first(0)));
	// A stored block whose length's complement does not match.
	CHECK_FALSE(Inflater.Inflate(FDeflateWriter().Bits(1, 1).Bits(0, 2).Bits(0, 5).Bits(4, 16).Bits(0x1234, 16).Bits(0x61626364, 32).Bytes(), Output));
	// A dynamic block naming more literal codes than there are.
	CHECK_FALSE(Inflater.Inflate(FDeflateWriter().Bits(1, 1).Bits(2, 2).Bits(30, 5).Bits(0, 5).Bits(15, 4).Bits(0, 32).Bits(0, 32).Bytes(), Output));
}

TEST_CASE("LZMA streams by liblzma unpack to their data", "[Chd]")
{
	for (const FVector& Vector : LzmaVectors())
	{
		INFO(Vector.Name);
		FLzmaDecoder Decoder(Vector.Parameters[0], Vector.Parameters[1], Vector.Parameters[2]);
		std::vector<uint8> Packed = FromHex(Vector.Hex);
		std::vector<uint8> Output(static_cast<std::size_t>(Vector.OutputBytes));
		REQUIRE(Decoder.Decode(Packed, Output));
		CHECK(Fnv1a(Output) == Vector.OutputHash);

		// The end mark must come exactly where the output ends.
		std::vector<uint8> Larger(Output.size() + 1);
		CHECK_FALSE(Decoder.Decode(Packed, Larger));
		CHECK_FALSE(Decoder.Decode(Packed, std::span(Output).first(Output.size() - 1)));
		CHECK_FALSE(Decoder.Decode(std::span(Packed).first(Packed.size() - 3), Output));
		Packed[0] = 1;
		CHECK_FALSE(Decoder.Decode(Packed, Output));
	}
}

TEST_CASE("FLAC frames by FFmpeg decode to their samples", "[Chd]")
{
	FFlacDecoder Decoder;
	for (const FVector& Vector : FlacVectors())
	{
		INFO(Vector.Name);
		const std::vector<uint8> Frames = FromHex(Vector.Hex);
		const bool bBigEndian = Vector.Parameters[0] != 0;
		std::vector<uint8> Samples(static_cast<std::size_t>(Vector.OutputBytes));
		const auto Taken = Decoder.DecodeStereo16(Frames, Samples, bBigEndian);
		REQUIRE(Taken);
		CHECK(*Taken == Frames.size());
		CHECK(Fnv1a(Samples) == Vector.OutputHash);

		// Fewer samples end with the frame that holds the last of them: before the last frame, for a stream of 1024-sample frames.
		std::vector<uint8> Fewer(Samples.size() / 2);
		const auto Part = Decoder.DecodeStereo16(Frames, Fewer, bBigEndian);
		REQUIRE(Part);
		CHECK(*Part <= Frames.size());
		CHECK((Vector.Name != "Independent" || *Part < Frames.size()));
		CHECK(std::equal(Fewer.begin(), Fewer.end(), Samples.begin()));

		CHECK_FALSE(Decoder.DecodeStereo16(std::span(Frames).first(Frames.size() - 1), Samples, bBigEndian));
		std::vector<uint8> Damaged = Frames;
		Damaged[Damaged.size() / 2] ^= 0x08;
		CHECK_FALSE(Decoder.DecodeStereo16(Damaged, Samples, bBigEndian));
	}
}

TEST_CASE("Zstandard frames by the reference tool unpack to their data", "[Chd]")
{
	FZstdDecoder Decoder;
	for (const FVector& Vector : ZstdVectors())
	{
		INFO(Vector.Name);
		const std::vector<uint8> Packed = FromHex(Vector.Hex);
		std::vector<uint8> Output(static_cast<std::size_t>(Vector.OutputBytes));
		REQUIRE(Decoder.Decode(Packed, Output));
		CHECK(Fnv1a(Output) == Vector.OutputHash);

		std::vector<uint8> Larger(Output.size() + 1);
		CHECK_FALSE(Decoder.Decode(Packed, Larger));
		CHECK_FALSE(Decoder.Decode(Packed, std::span(Output).first(Output.size() - 1)));
		CHECK_FALSE(Decoder.Decode(std::span(Packed).first(Packed.size() - 1), Output));

		// Damage is found, or at least never taken for the data.
		std::vector<uint8> Damaged = Packed;
		Damaged[Damaged.size() / 2] ^= 0x10;
		CHECK((!Decoder.Decode(Damaged, Output) || Fnv1a(Output) != Vector.OutputHash));
	}
}
