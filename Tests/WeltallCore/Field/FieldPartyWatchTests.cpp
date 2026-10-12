#include "Field/EventContext.h"
#include "Field/FieldArchive.h"
#include "Field/FieldPartyWatch.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	constexpr uint8 Fei = 0;
	constexpr uint8 Elly = 1;
	constexpr uint8 Citan = 2;
	constexpr uint8 Nobody = 0xFF;

	/** A map of four actors: the party's three, the first the pad's and the others following it, and someone else. */
	struct FWalk
	{
		FWalk()
		{
			Context.States.resize(4);
			Context.Actors.resize(4);
			for (FEventActorState& State : Context.States)
			{
				State.Reset(0);
			}
			Context.PartyCharacters = {Fei, Citan, Elly};
			Context.PartyActors = {0, 1, 2};
			Context.States[0].Flags0 |= EventActorFlags0::PadControlled;
			Follow(1, true);
			Follow(2, true);
		}

		/** A member's control step marks it as a follower this tick, or its actor runs something else. */
		void Follow(int32 Actor, bool bFollows)
		{
			uint32& Flags = Context.States[static_cast<std::size_t>(Actor)].Flags0;
			Flags = bFollows ? Flags | EventActorFlags0::PartyFollower : Flags & ~EventActorFlags0::PartyFollower;
		}

		bool Tick()
		{
			Party.Watch(Context);
			return Party.Together();
		}

		FEventContext Context;
		FFieldPartyWatch Party;
	};
}

TEST_CASE("The party is together while the pad walks its first member and every other member follows", "[Field]")
{
	FWalk Walk;
	CHECK(FFieldPartyWatch::Follows(Walk.Context));
	CHECK(Walk.Tick());

	// A party of one has nobody to wait for; a slot's actor left over from a member gone does not count.
	FWalk Alone;
	Alone.Context.PartyCharacters = {Fei, Nobody, Nobody};
	Alone.Follow(1, false);
	Alone.Follow(2, false);
	CHECK(Alone.Tick());
}

TEST_CASE("A member set apart, hidden or without an actor leaves the party apart, and so does a leader the pad does not walk", "[Field]")
{
	// Seated on a bed while the leader walks the cell: its actor runs a script of its own.
	FWalk Seated;
	Seated.Follow(1, false);
	CHECK_FALSE(Seated.Tick());

	FWalk Hidden;
	Hidden.Context.Actors[2].Flags |= FieldActorFlag::Hidden;
	CHECK_FALSE(Hidden.Tick());

	FWalk Absent;
	Absent.Context.PartyActors[2] = FEventContext::NoActor;
	CHECK_FALSE(Absent.Tick());

	// A memory played without the leader; the pad handed to someone else for a while.
	FWalk Leaderless;
	Leaderless.Context.PartyActors[0] = FEventContext::NoActor;
	CHECK_FALSE(Leaderless.Tick());

	FWalk Unwalked;
	Unwalked.Context.States[0].Flags0 &= ~EventActorFlags0::PadControlled;
	CHECK_FALSE(Unwalked.Tick());

	FWalk HandedOn;
	HandedOn.Context.Controls.bLeaderApart = true;
	CHECK_FALSE(HandedOn.Tick());
}

TEST_CASE("A message shown at a member of the party sets it apart; one at anybody else does not", "[Field]")
{
	FWalk Walk;
	FFieldMessageWindow& Window = Walk.Context.Messages.Windows[0];
	Window.bOpen = true;
	// Someone of the map speaks to the party as it stands behind its leader.
	Window.Owner = 3;
	Window.Speaker = 3;
	CHECK(Walk.Tick());
	// A follower answers; then the leader.
	Window.Speaker = 2;
	CHECK_FALSE(Walk.Tick());
	Window.Speaker = 0;
	CHECK_FALSE(Walk.Tick());
	// A window closed keeps its speaker's number and counts no more.
	Window.bOpen = false;
	CHECK(FFieldPartyWatch::Follows(Walk.Context));

	// An actor left in a slot the party has nobody in is nobody's.
	FWalk Two;
	Two.Context.PartyCharacters = {Fei, Citan, Nobody};
	Two.Context.Messages.Windows[0].bOpen = true;
	Two.Context.Messages.Windows[0].Speaker = 2;
	CHECK(Two.Tick());
}

TEST_CASE("A party apart is together again only after its members have followed a while; a map's first picture shows it as it is", "[Field]")
{
	FWalk Walk;
	REQUIRE(Walk.Tick());
	Walk.Follow(2, false);
	CHECK_FALSE(Walk.Tick());
	Walk.Follow(2, true);
	for (int32 Tick = 0; Tick < FFieldPartyWatch::EndTicks - 1; ++Tick)
	{
		CHECK_FALSE(Walk.Tick());
	}
	CHECK(Walk.Tick());
	// A moment apart in the middle starts the count again.
	Walk.Follow(1, false);
	CHECK_FALSE(Walk.Tick());
	Walk.Follow(1, true);
	CHECK_FALSE(Walk.Tick());
	Walk.Follow(1, false);
	CHECK_FALSE(Walk.Tick());
	Walk.Follow(1, true);
	for (int32 Tick = 0; Tick < FFieldPartyWatch::EndTicks - 1; ++Tick)
	{
		CHECK_FALSE(Walk.Tick());
	}
	CHECK(Walk.Tick());

	// A map whose scripts have not run its members yet, then do before its first picture: nothing waits.
	FWalk Loading;
	Loading.Context.SettleTicks = FEventContext::MapStartSettleTicks;
	Loading.Follow(1, false);
	CHECK_FALSE(Loading.Tick());
	Loading.Follow(1, true);
	CHECK(Loading.Tick());

	// The field's first tick back from a battle or the menu shows the party as it stands.
	FWalk Back;
	Back.Follow(1, false);
	REQUIRE_FALSE(Back.Tick());
	Back.Party.Reset();
	Back.Follow(1, true);
	CHECK(Back.Tick());
}
