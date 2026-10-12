#include "Game/PsxRandom.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

TEST_CASE("The PlayStation's rand() gives its sequence from seed 0", "[Game]")
{
	FPsxRandom Random;
	CHECK(Random.Next() == 0);
	CHECK(Random.Next() == 21468);
	CHECK(Random.Next() == 9988);
	CHECK(Random.NextByte() == (22117 & 0xFF));
	CHECK(Random.Seed() == 0xD6651C2Cu);
}
