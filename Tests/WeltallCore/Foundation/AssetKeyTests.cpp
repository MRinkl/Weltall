#include "Foundation/AssetKey.h"

#include <catch_amalgamated.hpp>

#include <string>

using namespace Weltall;

TEST_CASE("Asset keys name their files with any description before them", "[Foundation][AssetKey]")
{
	CHECK(AssetKeyName(0x0123456789ABCDEFull) == "0123456789abcdef");
	CHECK(AssetKeyOfName("0123456789abcdef") == 0x0123456789ABCDEFull);
	CHECK(AssetKeyOfName("lahan wall-0123456789ABCDEF") == 0x0123456789ABCDEFull);
	CHECK(AssetKeyOfName("door_0123456789abcdef") == 0x0123456789ABCDEFull);
	CHECK_FALSE(AssetKeyOfName("10123456789abcdef"));
	CHECK_FALSE(AssetKeyOfName("0123456789abcdeg"));
	CHECK_FALSE(AssetKeyOfName("abcdef"));
}

TEST_CASE("An asset's key is the start of its SHA-1", "[Foundation][AssetKey]")
{
	const std::string Text = "abc";
	CHECK(AssetKeyOf(std::span(reinterpret_cast<const uint8*>(Text.data()), Text.size())) == 0xA9993E364706816Aull);
}
