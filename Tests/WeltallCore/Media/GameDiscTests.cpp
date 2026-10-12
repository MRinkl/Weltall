#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	void CheckBootFiles(const FIso9660& Disc, const char* Serial)
	{
		// Everything else on the disc is reached through the game's own file index.
		REQUIRE(Disc.Entries().size() == 2);
		CHECK(Disc.Find("SYSTEM.CNF") != nullptr);
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		CHECK(*FindBootExecutable(Disc) == Serial);
		CHECK(Executable->LoadAddress() == 0x80010000);
		CHECK(Executable->InitialPc() == 0x80019524);
		CHECK(Executable->Body().size() == 0x49800);
	}
}

TEST_CASE("Disc 1 is identified and holds only its boot files", "[Media][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Identity = IdentifyDisc(*Disc);
	REQUIRE(Identity);
	CHECK(Identity->DiscNumber == 1);
	CheckBootFiles(*Disc, "SLUS_006.64");
}

TEST_CASE("Disc 2 is identified and holds only its boot files", "[Media][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	const auto Identity = IdentifyDisc(*Disc);
	REQUIRE(Identity);
	CHECK(Identity->DiscNumber == 2);
	CheckBootFiles(*Disc, "SLUS_006.69");
}
