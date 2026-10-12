#include "Gears/GearShopModel.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

TEST_CASE("The gear shop's model runs three ticks every other frame, or the field's pace when asked", "[Gears]")
{
	FGearShopModel Original;
	REQUIRE_FALSE(Original.Open());
	REQUIRE_FALSE(Original.Frame());
	// The shop's word leaves the counter negative: no tick, then three.
	CHECK(Original.Models().VblankCounter == FGearShopModel::OriginalExtraVblanks + 1);
	const uint16 Phase = Original.Models().WindPhase;
	REQUIRE_FALSE(Original.Frame());
	CHECK(Original.Models().VblankCounter == 0);
	const uint16 ThreeTicks = static_cast<uint16>(Original.Models().WindPhase - Phase);
	CHECK(ThreeTicks != 0);

	FGearShopModelOptions Options;
	Options.bFieldPace = true;
	FGearShopModel Paced;
	REQUIRE_FALSE(Paced.Open(Options));
	REQUIRE_FALSE(Paced.Frame());
	CHECK(Paced.Models().VblankCounter == 1);
	CHECK(Paced.Models().WindPhase == 0);
	REQUIRE_FALSE(Paced.Frame());
	CHECK(Paced.Models().VblankCounter == 0);
	CHECK(static_cast<uint16>(3 * Paced.Models().WindPhase) == ThreeTicks);
}

TEST_CASE("The gear shop shows no gear without a model", "[Gears]")
{
	FGearShopModel Model;
	REQUIRE_FALSE(Model.Open());
	const auto Error = Model.Show(FGearShopGear{}, FGearFiles{});
	REQUIRE(Error);
	CHECK(Model.Shown() == nullptr);
}
