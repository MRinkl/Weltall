#include "Field/FieldGears.h"
#include "Field/FieldPlay.h"
#include "Field/FieldRequests.h"
#include "Gears/GearModels.h"
#include "Support/FieldPlaySession.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

// The field's gears where the field meets the rest of the game: a map change, a movie, a return from
// a battle, as a new game plays them on real maps.

namespace
{
	/** Map 1: Lahan, no gears. */
	constexpr int32 LahanMap = 1;
	/** Map 16: the mountain path, where four gears stand. */
	constexpr int32 MountainPathMap = 16;
	constexpr int32 MountainPathGears = 4;
	constexpr int32 SettledTicks = 40;

	FEventContext& ContextOf(Testing::FFieldPlaySession& Session)
	{
		return const_cast<FFieldRuntime&>(Session.Play->Field()).Context();
	}

	FFieldRuntime& RuntimeOf(Testing::FFieldPlaySession& Session)
	{
		return const_cast<FFieldRuntime&>(Session.Play->Field());
	}

	/** The scripts' request for map Number, then the ticks the change takes and the next map's first ones. */
	void ChangeMap(Testing::FFieldPlaySession& Session, int32 Number)
	{
		FEventContext& Context = ContextOf(Session);
		Context.MapNumber = Number;
		Context.Requests.bMapChange = true;
		const int32 Loads = Session.Play->Field().MapLoads();
		for (int32 Tick = 0; Tick < 100 && Session.Play->Field().MapLoads() == Loads; ++Tick)
		{
			Session.Run(1);
		}
		REQUIRE(Session.Play->Field().MapLoads() == Loads + 1);
		Session.Run(SettledTicks);
	}
}

TEST_CASE("A map change loads the next map's gears and lets the last map's go", "[Field][Gears][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Testing::EnterFieldMap(*Disc, LahanMap);
	Session->Run(SettledTicks);
	CHECK_FALSE(ContextOf(*Session).Gears.Models);
	ChangeMap(*Session, MountainPathMap);
	const FEventContext& Context = ContextOf(*Session);
	REQUIRE(Context.Gears.Models);
	CHECK(Context.Gears.Count == MountainPathGears);
	CHECK_FALSE(Session->Frame.Gears.Links.empty());
	ChangeMap(*Session, LahanMap);
	CHECK_FALSE(ContextOf(*Session).Gears.Models);
	CHECK(Session->Frame.Gears.Objects.empty());
	CHECK(Session->Frame.Gears.Links.empty());
}

TEST_CASE("A movie frees the gears' models, and its end loads them again from their first script", "[Field][Gears][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Testing::EnterFieldMap(*Disc, MountainPathMap);
	Session->Run(SettledTicks);
	FEventContext& Context = ContextOf(*Session);
	const int32 Loads = Context.Gears.Loads;
	const uint32 Layout = FieldGearLayout(Context.Gears);
	Context.Requests.bMovie = true;
	for (int32 Tick = 0; Tick < 20 && Session->Play->Field().Request().Kind != EFieldRequest::Movie; ++Tick)
	{
		Session->Run(1);
	}
	REQUIRE(Session->Play->Field().Request().Kind == EFieldRequest::Movie);
	CHECK_FALSE(Context.Gears.Models);
	REQUIRE_FALSE(RuntimeOf(*Session).ResumeAfterRequest());
	REQUIRE(Context.Gears.Models);
	CHECK(Context.Gears.Loads == Loads + 1);
	CHECK(FieldGearLayout(Context.Gears) != Layout);
	for (int32 Slot = 0; Slot < MountainPathGears; ++Slot)
	{
		CHECK(Context.Gears.Models->Model(Slot) != nullptr);
	}
	Session->Run(SettledTicks);
	CHECK(Session->Frame.Gears.Layout == FieldGearLayout(Context.Gears));
	CHECK_FALSE(Session->Frame.Gears.Links.empty());
}

TEST_CASE("The field's return from a battle plays every gear's kept animation again", "[Field][Gears][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Testing::EnterFieldMap(*Disc, MountainPathMap);
	FEventContext& Context = ContextOf(*Session);
	REQUIRE(Context.Gears.Models);
	// What the field kept of its gears before the battle.
	Context.Gears.Animations[1] = 3;
	REQUIRE_FALSE(ReplayFieldGearAnimations(Context));
	CHECK(Context.Gears.Models->PlaySlot == MountainPathGears - 1);
	CHECK(Context.Gears.Models->PlayMask == 0);
	CHECK_FALSE(Context.Gears.Models->Fault);
}
