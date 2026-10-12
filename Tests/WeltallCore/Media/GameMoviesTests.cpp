#include "Media/GameMovies.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	FGameFileIndex ReadIndex(const FIso9660& Disc)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		return *Index;
	}
}

TEST_CASE("Disc 1's movies are its movie group's files, the boot's logos movie 16 among them", "[Media][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FGameFileIndex Index = ReadIndex(*Disc);
	const auto First = FindMovie(Index, 0);
	REQUIRE(First);
	CHECK(First->FirstLba == 41);
	CHECK(First->SectorCount == 784);
	const auto Desert = FindMovie(Index, 7);
	REQUIRE(Desert);
	CHECK(Desert->SectorCount == 10696);
	const auto Logos = FindMovie(Index, 16);
	REQUIRE(Logos);
	CHECK(Logos->SectorCount == 2456);
	CHECK_FALSE(FindMovie(Index, 17));
	CHECK_FALSE(FindMovie(Index, -1));
}

TEST_CASE("Disc 2 has twelve movies, its boot's logos movie 7, the file disc 1 boots with", "[Media][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	const FGameFileIndex Index = ReadIndex(*Disc);
	const auto Logos = FindMovie(Index, 7);
	REQUIRE(Logos);
	CHECK(Logos->SectorCount == 2456);
	CHECK(FindMovie(Index, 11));
	CHECK_FALSE(FindMovie(Index, 12));
	CHECK_FALSE(FindMovie(Index, 16));
}
