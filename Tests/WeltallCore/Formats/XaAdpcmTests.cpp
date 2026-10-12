#include "Formats/XaAdpcm.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	/** A sector's sound with every group's block headers set to Header and every data byte to Data. */
	std::vector<uint8> MakeSound(uint8 Header, uint8 Data)
	{
		std::vector<uint8> Sound(XaSector::SoundBytes);
		for (std::size_t Group = 0; Group < XaSector::GroupCount; ++Group)
		{
			uint8* Bytes = Sound.data() + Group * XaSector::GroupBytes;
			for (std::size_t Index = 0; Index < 16; ++Index)
			{
				Bytes[Index] = Header;
			}
			for (std::size_t Index = 16; Index < XaSector::GroupBytes; ++Index)
			{
				Bytes[Index] = Data;
			}
		}
		return Sound;
	}
}

TEST_CASE("XA coding info gives channels, rate and sample size", "[Formats]")
{
	const FXaCodingInfo StereoFull = FXaCodingInfo::FromSubheader(0x01);
	CHECK(StereoFull.bStereo);
	CHECK(StereoFull.SampleRate() == 37800);
	CHECK_FALSE(StereoFull.bEightBit);
	const FXaCodingInfo MonoHalfEight = FXaCodingInfo::FromSubheader(0x14);
	CHECK_FALSE(MonoHalfEight.bStereo);
	CHECK(MonoHalfEight.SampleRate() == 18900);
	CHECK(MonoHalfEight.bEightBit);
}

TEST_CASE("XA 4-bit mono without a filter shifts each nibble into place", "[Formats]")
{
	// Shift 12 leaves each nibble as it is; low nibbles 3 and high nibbles -2 (0xE).
	std::array<FXaChannelHistory, 2> History{};
	std::vector<int16> Out;
	DecodeXaSector(MakeSound(0x0C, 0xE3), FXaCodingInfo{}, History, Out);
	REQUIRE(Out.size() == XaSector::GroupCount * 8 * 28);
	// Block 0 takes the low nibble of each word's first byte, block 1 its high nibble.
	CHECK(Out[0] == 3);
	CHECK(Out[27] == 3);
	CHECK(Out[28] == -2);
	CHECK(Out[8 * 28] == 3);
}

TEST_CASE("XA filters weigh the two previous samples in 64ths", "[Formats]")
{
	// Filter 1 adds 60/64 of the previous sample; shift 0 makes nibble 1 into 4096.
	std::array<FXaChannelHistory, 2> History{};
	History[0].Old = 640;
	std::vector<int16> Out;
	DecodeXaSector(MakeSound(0x10, 0x11), FXaCodingInfo{}, History, Out);
	CHECK(Out[0] == 4096 + (640 * 60 + 32) / 64);
	CHECK(Out[1] == 4096 + (Out[0] * 60 + 32) / 64);
	CHECK(History[0].Old == Out.back());
}

TEST_CASE("XA stereo interleaves even blocks left and odd blocks right", "[Formats]")
{
	std::array<FXaChannelHistory, 2> History{};
	std::vector<int16> Out;
	DecodeXaSector(MakeSound(0x0C, 0x71), FXaCodingInfo::FromSubheader(0x01), History, Out);
	REQUIRE(Out.size() == XaSector::GroupCount * 8 * 28);
	CHECK(Out[0] == 1);
	CHECK(Out[1] == 7);
	CHECK(Out[2] == 1);
	CHECK(History[0].Old == 1);
	CHECK(History[1].Old == 7);
}

TEST_CASE("XA 8-bit samples take a whole byte of each word", "[Formats]")
{
	std::array<FXaChannelHistory, 2> History{};
	std::vector<int16> Out;
	DecodeXaSector(MakeSound(0x08, 0xF0), FXaCodingInfo::FromSubheader(0x10), History, Out);
	REQUIRE(Out.size() == XaSector::GroupCount * 4 * 28);
	CHECK(Out[0] == -16);
}
