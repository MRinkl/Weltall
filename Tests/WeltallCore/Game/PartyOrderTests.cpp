#include "Game/PartyOrder.h"

#include <catch_amalgamated.hpp>

#include <array>

using namespace Weltall;

namespace
{
	constexpr uint8 None = 0xFF;
	constexpr uint8 Fei = 0;
	constexpr uint8 Elly = 1;
	constexpr uint8 Citan = 2;
	constexpr uint8 Bart = 3;
	constexpr uint8 Emeralda = 6;
	constexpr uint8 SwordCitan = 9;
	constexpr uint8 GrownEmeralda = 10;
	/** A character past the party's records, as the story puts one in the party now and then: a guest no menu shows. */
	constexpr uint8 Guest = 11;
	constexpr int8 Nobody = FPartyOrder::Nobody;

	using FParty = std::array<uint8, 3>;

	FPartyOrder Arranged(const FParty& Party, std::initializer_list<uint8> Shown)
	{
		FPartyOrder Order;
		Order.Arrange(Party, std::span<const uint8>(Shown.begin(), Shown.size()));
		return Order;
	}
}

TEST_CASE("Until the player arranges the party it shows the story's order, slot for slot", "[Game]")
{
	const FPartyOrder Order;
	CHECK_FALSE(Order.IsSet());
	CHECK_FALSE(Order.IsFor({Fei, Elly, Citan}));
	CHECK(Order.PlacesOf({Fei, Elly, Citan}) == FPartyOrder::FPlaces{0, 1, 2});
	// Even with a gap, as the original's menus show one.
	CHECK(Order.PlacesOf({Fei, None, Citan}) == FPartyOrder::FPlaces{0, 1, 2});
	CHECK(Order.Shown({Citan, Fei, None}) == FParty{Citan, Fei, None});
}

TEST_CASE("An order is for the people it was arranged with: anyone joining or leaving shows the story's order", "[Game]")
{
	const FPartyOrder Order = Arranged({Fei, Elly, Citan}, {Elly, Fei, Citan});
	CHECK(Order.IsSet());
	CHECK(Order.IsFor({Fei, Elly, Citan}));
	CHECK(Order.PlacesOf({Fei, Elly, Citan}) == FPartyOrder::FPlaces{1, 0, 2});
	CHECK(Order.Shown({Fei, Elly, Citan}) == FParty{Elly, Fei, Citan});
	// Elly leaves, or Bart joins in her place: the story's order, whatever was arranged.
	CHECK_FALSE(Order.IsFor({Fei, Citan, None}));
	CHECK(Order.Shown({Fei, Citan, None}) == FParty{Fei, Citan, None});
	CHECK(Order.Shown({Fei, Bart, Citan}) == FParty{Fei, Bart, Citan});
	CHECK(Order.Shown({Citan, Elly, None}) == FParty{Citan, Elly, None});
	// The same three again, in other slots of the game's party: the order arranged.
	CHECK(Order.Shown({Citan, Fei, Elly}) == FParty{Elly, Fei, Citan});
	// A gap among them comes last.
	const FPartyOrder Two = Arranged({Fei, None, Elly}, {Elly, Fei});
	CHECK(Two.PlacesOf({Fei, None, Elly}) == FPartyOrder::FPlaces{2, 0, Nobody});
}

TEST_CASE("The story's later Citan and Emeralda are the people the order was arranged with", "[Game]")
{
	const FPartyOrder Order = Arranged({Fei, Emeralda, Citan}, {Citan, Emeralda, Fei});
	CHECK(Order.Shown({Fei, GrownEmeralda, SwordCitan}) == FParty{SwordCitan, GrownEmeralda, Fei});
	CHECK(Arranged({Fei, SwordCitan, None}, {SwordCitan, Fei}) == Arranged({Fei, Citan, None}, {Citan, Fei}));
}

TEST_CASE("Arranging takes only the party's members, those left out after the others in the story's order", "[Game]")
{
	// Bart is not in the party; Citan is not named.
	const FPartyOrder Order = Arranged({Fei, Elly, Citan}, {Bart, Elly});
	CHECK(Order == Arranged({Fei, Elly, Citan}, {Elly, Fei, Citan}));
	// Arranging again is for the party arranged now; the earlier order is gone.
	FPartyOrder Again = Order;
	const std::array<uint8, 2> BartFirst{Bart, Fei};
	Again.Arrange({Fei, Bart, None}, BartFirst);
	CHECK(Again.Shown({Fei, Bart, None}) == FParty{Bart, Fei, None});
	CHECK(Again.Shown({Fei, Elly, Citan}) == FParty{Fei, Elly, Citan});
}

TEST_CASE("A party arranged in the story's own order keeps no order, which the story's later changes then show", "[Game]")
{
	CHECK_FALSE(Arranged({Fei, Elly, Citan}, {Fei, Elly, Citan}).IsSet());
	CHECK_FALSE(Arranged({Citan, Fei, None}, {Citan, Fei}).IsSet());
	// One member alone is always in the story's order.
	CHECK_FALSE(Arranged({Fei, None, None}, {Fei}).IsSet());
	CHECK_FALSE(Arranged({Bart, Guest, None}, {Guest, Bart}).IsSet());
}

TEST_CASE("A guest in the party is none of the people arranged and is shown after them", "[Game]")
{
	const FPartyOrder Order = Arranged({Fei, Guest, Citan}, {Citan, Fei});
	CHECK(Order.IsFor({Fei, Guest, Citan}));
	CHECK(Order.IsFor({Fei, Citan, None}));
	CHECK(Order.Shown({Fei, Guest, Citan}) == FParty{Citan, Fei, Guest});
}

TEST_CASE("A member swapped into the party takes the place of the one it replaces", "[Game]")
{
	FPartyOrder Order = Arranged({Fei, Elly, Citan}, {Elly, Fei, Citan});
	Order.Replace(Elly, Bart);
	CHECK(Order.Shown({Fei, Bart, Citan}) == FParty{Bart, Fei, Citan});
	CHECK_FALSE(Order.IsFor({Fei, Elly, Citan}));
	// Someone the order does not hold leaving, or a newcomer it already holds, changes nothing.
	const FPartyOrder Before = Order;
	Order.Replace(Elly, Emeralda);
	CHECK(Order == Before);
	Order.Replace(Citan, Fei);
	CHECK(Order == Before);
}

TEST_CASE("A party order reads back from a save's bytes, and bytes that hold none read as unset", "[Game]")
{
	const FPartyOrder Order = Arranged({Fei, Elly, Bart}, {Bart, Elly, Fei});
	const std::array<uint8, FPartyOrder::SavedBytes> Bytes = Order.ToSaved();
	CHECK(Bytes == std::array<uint8, FPartyOrder::SavedBytes>{3, Bart, Elly, Fei});
	CHECK(FPartyOrder::FromSaved(Bytes) == Order);
	CHECK(FPartyOrder::FromSaved(Arranged({Fei, Citan, None}, {Citan, Fei}).ToSaved()).Shown({Fei, Citan, None}) == FParty{Citan, Fei, None});
	const std::array<uint8, FPartyOrder::SavedBytes> Zeros{};
	CHECK_FALSE(FPartyOrder::FromSaved(Zeros).IsSet());
	const std::array<uint8, FPartyOrder::SavedBytes> Twice{2, Fei, Fei};
	CHECK_FALSE(FPartyOrder::FromSaved(Twice).IsSet());
	const std::array<uint8, FPartyOrder::SavedBytes> Alone{1, Fei};
	CHECK_FALSE(FPartyOrder::FromSaved(Alone).IsSet());
	const std::array<uint8, FPartyOrder::SavedBytes> TooMany{4, Bart, Fei, Citan};
	CHECK_FALSE(FPartyOrder::FromSaved(TooMany).IsSet());
	const std::array<uint8, FPartyOrder::SavedBytes> NoPerson{2, Fei, SwordCitan};
	CHECK_FALSE(FPartyOrder::FromSaved(NoPerson).IsSet());
}

TEST_CASE("Where some of the party ride and some walk, each member is shown in a slot that does as its own", "[Game]")
{
	const FParty Party{Fei, Citan, Bart};
	const FPartyOrder BartFirst = Arranged(Party, {Bart, Fei, Citan});
	// All on foot, or all in their gears: the order as it is.
	CHECK(BartFirst.ShownAsTheyRide(Party, {false, false, false}) == BartFirst.Shown(Party));
	CHECK(BartFirst.ShownAsTheyRide(Party, {true, true, true}) == BartFirst.Shown(Party));
	// Bart alone called his gear in a battle: his own slot, the third, rides, and shows him; the others walk in their order.
	CHECK(BartFirst.ShownAsTheyRide(Party, {false, false, true}) == FParty{Fei, Citan, Bart});
	// Fei and Bart ride, Citan has no gear: the riding slots show the riders in their player's order.
	const FPartyOrder CitanFirst = Arranged(Party, {Citan, Bart, Fei});
	CHECK(CitanFirst.ShownAsTheyRide(Party, {true, false, true}) == FParty{Bart, Citan, Fei});
	// An empty slot stays empty, the members about it in their order.
	const FPartyOrder EllyFirst = Arranged({Fei, None, Elly}, {Elly, Fei});
	CHECK(EllyFirst.ShownAsTheyRide({Fei, None, Elly}, {false, false, false}) == FParty{Elly, None, Fei});
	// No order kept, or one for other people: everyone its own.
	CHECK(FPartyOrder().ShownAsTheyRide(Party, {true, false, false}) == Party);
	CHECK(EllyFirst.ShownAsTheyRide(Party, {true, false, false}) == Party);
}
