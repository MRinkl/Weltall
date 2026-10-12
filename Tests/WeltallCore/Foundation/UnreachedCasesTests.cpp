#include "Foundation/UnreachedCases.h"

#include <catch_amalgamated.hpp>

#include <string>
#include <vector>

using namespace Weltall;

TEST_CASE("The cases no script reaches are carried past and noted once, unless the record stops or there is none", "[Foundation]")
{
	FUnreachedCases Cases;
	CHECK(Cases.Recover("a fifth gear"));
	CHECK(Cases.Recover("a fifth gear"));
	CHECK(Cases.Recover("a sound channel past the scripts' eight"));
	CHECK(Cases.TakeNoted() == std::vector<std::string>{"a fifth gear", "a sound channel past the scripts' eight"});
	CHECK(Cases.TakeNoted().empty());
	CHECK(Cases.Recover("a fifth gear"));
	CHECK(Cases.TakeNoted().empty());

	// Stopping, as for development, or with no record at all, as the tests run.
	Cases.bStops = true;
	CHECK_FALSE(Cases.Recover("a map change with a transition past the seven"));
	CHECK(Cases.TakeNoted().empty());
	CHECK_FALSE(RecoverUnreached(nullptr, "a fifth gear"));
	Cases.bStops = false;
	CHECK(RecoverUnreached(&Cases, "a fifth gear"));
}
