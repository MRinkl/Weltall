#include "Foundation/LittleEndian.h"
#include "Foundation/Sha1.h"
#include "Graphics/Gte.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"
#include "Support/XorShift32.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	void AddWord(FSha1& Hash, int32 Value)
	{
		const uint32 Bits = static_cast<uint32>(Value);
		const std::array<uint8, 4> Bytes = {static_cast<uint8>(Bits), static_cast<uint8>(Bits >> 8), static_cast<uint8>(Bits >> 16), static_cast<uint8>(Bits >> 24)};
		Hash.Update(Bytes);
	}
}

TEST_CASE("libgte's square root rounds down through its table", "[Graphics]")
{
	CHECK(SquareRoot0(0) == 0);
	CHECK(SquareRoot0(1) == 1);
	CHECK(SquareRoot0(3) == 1);
	CHECK(SquareRoot0(4) == 2);
	CHECK(SquareRoot0(100) == 10);
	CHECK(SquareRoot0(0x10000) == 0x100);
	CHECK(SquareRoot0(0x7FFFFFFF) == 46152);
	CHECK(GteSquare(0x18000) == 0x8000 * 0x8000);
	CHECK(GteSquare(-3) == 9);
}

// Digests of the kernel's own ratan2 and SquareRoot0 on the same arguments, run under the oracle
// harness.
TEST_CASE("libgte's arctangent and square root give what the kernel's give", "[Graphics][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Arctangents = FGteArctangents::Read(*Executable);
	REQUIRE(Arctangents);
	Testing::FXorShift32 Random(0x6A7E);
	FSha1 Angles;
	for (int32 Case = 0; Case < 4000; ++Case)
	{
		const int32 YWord = static_cast<int32>(Random.Next());
		const int32 Y = YWord >> (Random.Next() % 31);
		const int32 XWord = static_cast<int32>(Random.Next());
		const int32 X = XWord >> (Random.Next() % 31);
		AddWord(Angles, GteRatan2(Y, X, *Arctangents));
	}
	FSha1 Roots;
	for (int32 Case = 0; Case < 4000; ++Case)
	{
		const uint32 Word = Random.Next();
		const int32 Value = static_cast<int32>(Word >> (1 + Random.Next() % 31));
		AddWord(Roots, SquareRoot0(Value));
	}
	CHECK(FSha1::ToHex(Angles.Finish()) == "213687f801d21d402e40b7d33d2f27e7025bfc96");
	CHECK(FSha1::ToHex(Roots.Finish()) == "668d4160d0060a6cc9f5fe2da4be5328614a5e57");
	CHECK(GteRatan2(0, 0, *Arctangents) == 0);
	CHECK(GteRatan2(1, 0, *Arctangents) == 0x400);
	CHECK(GteRatan2(0, -1, *Arctangents) == 0x800);
	CHECK(GteRatan2(-1, -1, *Arctangents) == -0x600);
}

TEST_CASE("The kernel's sine table holds libgte's rounded sines and cosines", "[Graphics][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	// Pairs of sine and cosine for each of the 4096 angles.
	const auto Table = Executable->Read(0x800523F0, 4096 * 4);
	REQUIRE(Table.size() == 4096 * 4);
	for (int32 Angle = 0; Angle < 4096; ++Angle)
	{
		INFO("angle " << Angle);
		CHECK(LittleEndian::ReadS16(Table, static_cast<std::size_t>(Angle) * 4) == GteSin(Angle));
		CHECK(LittleEndian::ReadS16(Table, static_cast<std::size_t>(Angle) * 4 + 2) == GteCos(Angle));
	}
	// libgte's square root table, which the port computes.
	const auto Roots = Executable->Read(0x80056A00, 192 * 2);
	REQUIRE(Roots.size() == 192 * 2);
	for (int32 Index = 0; Index < 192; ++Index)
	{
		const int32 Normalized = Index + 64;
		// The root of n << 18 (n from 64 to 255) is the table's entry for n.
		CHECK(SquareRoot0(Normalized << 18) == LittleEndian::ReadS16(Roots, static_cast<std::size_t>(Index) * 2));
	}
}
