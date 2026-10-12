#include "Gears/GearChannels.h"
#include "Gears/GearModels.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

TEST_CASE("The gears' channel pool hands out the lowest free channel and fails when full", "[Gears]")
{
	FGearChannelPool Pool;
	Pool.Init(3, false);
	const int32 First = Pool.Allocate();
	Pool[First].bUsed = true;
	const int32 Second = Pool.Allocate();
	Pool[Second].bUsed = true;
	const int32 Third = Pool.Allocate();
	Pool[Third].bUsed = true;
	CHECK((First == 0 && Second == 1 && Third == 2));
	CHECK(Pool.Allocate() == FGearChannelPool::None);
	Pool.Free(Second);
	CHECK(Pool.LowestFree() == 1);
	CHECK(Pool.Allocate() == 1);
	CHECK(Pool.LowestFree() == 3);
}

TEST_CASE("A growing channel pool hands out more channels than it started with", "[Gears]")
{
	FGearChannelPool Pool;
	Pool.Init(1, true);
	Pool[Pool.Allocate()].bUsed = true;
	const int32 Grown = Pool.Allocate();
	CHECK(Grown == 1);
	CHECK(Pool.Capacity() == 2);
}

TEST_CASE("The gears run a tick for every two vblanks, the field's one a call and the gear shop's three every other", "[Gears]")
{
	FGearModels Field;
	REQUIRE_FALSE(Field.Init(0x40));
	for (int32 Call = 1; Call <= 3; ++Call)
	{
		REQUIRE_FALSE(Field.Tick(1));
		CHECK(Field.VblankCounter == 0);
		CHECK(Field.WindPhase == 56 * Call);
	}
	// The gear shop passes a word that is no vblank count: its sign bit leaves the counter negative, no tick
	// runs, and the next call's sum wraps past the cap of three ticks.
	constexpr uint32 ShopExtraVblanks = 0x801CB480;
	FGearModels Shop;
	REQUIRE_FALSE(Shop.Init(0x40));
	REQUIRE_FALSE(Shop.Tick(ShopExtraVblanks));
	CHECK(Shop.VblankCounter == 0x801CB481);
	CHECK(Shop.WindPhase == 0);
	REQUIRE_FALSE(Shop.Tick(ShopExtraVblanks));
	CHECK(Shop.VblankCounter == 0);
	CHECK(Shop.WindPhase == 168);
}

TEST_CASE("The gears' module refuses a pool of no channels", "[Gears]")
{
	FGearModels Gears;
	CHECK(Gears.Init(0));
}
