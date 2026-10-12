#include "Foundation/Sha1.h"

#include <catch_amalgamated.hpp>

#include <string_view>
#include <vector>

using namespace Weltall;

namespace
{
	std::string HashText(std::string_view Text)
	{
		return FSha1::ToHex(FSha1::Hash(std::span(reinterpret_cast<const uint8*>(Text.data()), Text.size())));
	}
}

TEST_CASE("SHA-1 matches the FIPS 180 examples", "[Foundation]")
{
	CHECK(HashText("") == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
	CHECK(HashText("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d");
	CHECK(HashText("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") == "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
}

TEST_CASE("SHA-1 gives the same digest however the input is split", "[Foundation]")
{
	const std::vector<uint8> Million(1000000, static_cast<uint8>('a'));
	FSha1 Incremental;
	for (std::size_t Offset = 0; Offset < Million.size(); Offset += 997)
	{
		Incremental.Update(std::span(Million).subspan(Offset, std::min<std::size_t>(997, Million.size() - Offset)));
	}
	CHECK(FSha1::ToHex(Incremental.Finish()) == "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
}
