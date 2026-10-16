#include "Formats/Wave.h"

#include <catch_amalgamated.hpp>

#include <algorithm>

using namespace Weltall;

TEST_CASE("PCM WAVE preserves stereo samples and sampler loop endpoints", "[Wave][Assets]")
{
	FWave Wave;
	Wave.SampleRate = 48000;
	Wave.Samples = {-32768, 32767, -1, 1, 100, -100, 0, 0};
	Wave.Loop = FWaveLoop{1, 3};
	const auto Bytes = Wave.Serialize();
	REQUIRE(Bytes);
	const auto Parsed = FWave::Parse(*Bytes);
	REQUIRE(Parsed);
	CHECK(Parsed->Samples == Wave.Samples);
	CHECK(Parsed->SampleRate == 48000);
	CHECK(Parsed->Channels == 2);
	REQUIRE(Parsed->Loop);
	CHECK(Parsed->Loop->FirstFrame == 1);
	CHECK(Parsed->Loop->EndFrame == 3);
}

TEST_CASE("PCM WAVE validates chunks and complete frames before decoding", "[Wave][Assets]")
{
	FWave Wave;
	Wave.Channels = 1;
	Wave.Samples = {1, 2, 3};
	const auto Bytes = Wave.Serialize();
	REQUIRE(Bytes);
	SECTION("truncated sample data")
	{
		auto Broken = *Bytes;
		Broken.pop_back();
		CHECK_FALSE(FWave::Parse(Broken));
	}
	SECTION("a chunk size overflows the file")
	{
		auto Broken = *Bytes;
		Broken[40] = 0xFF;
		Broken[41] = 0xFF;
		Broken[42] = 0xFF;
		Broken[43] = 0xFF;
		CHECK_FALSE(FWave::Parse(Broken));
	}
	SECTION("compressed audio")
	{
		auto Broken = *Bytes;
		Broken[20] = 3;
		CHECK_FALSE(FWave::Parse(Broken));
	}
	SECTION("out of range sampler loop")
	{
		Wave.Loop = FWaveLoop{1, 4};
		CHECK_FALSE(Wave.Serialize());
	}
	SECTION("audio editor metadata with an odd byte count")
	{
		const std::vector<uint8> Chunk{'J', 'U', 'N', 'K', 1, 0, 0, 0, 42, 0};
		std::vector<uint8> WithMetadata(Bytes->size() + Chunk.size());
		std::copy_n(Bytes->begin(), 12, WithMetadata.begin());
		std::copy(Chunk.begin(), Chunk.end(), WithMetadata.begin() + 12);
		std::copy(Bytes->begin() + 12, Bytes->end(), WithMetadata.begin() + 12 + Chunk.size());
		WithMetadata[4] = static_cast<uint8>(WithMetadata[4] + Chunk.size());
		const auto Parsed = FWave::Parse(WithMetadata);
		REQUIRE(Parsed);
		CHECK(Parsed->Samples == Wave.Samples);
	}
}
