#include "Field/EventMachine.h"
#include "Game/FieldProgram.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <memory>
#include <optional>
#include <string>

using namespace Weltall;

// Doors that open when the party walks into them, in Lahan. The tavern's door to the village stands down and left of
// where a new game's party starts, straight along one of the pad's diagonals, at the end of a passage as wide as
// itself. The door of the house beside it stands in a long wall the party can slide along.

namespace
{
	constexpr uint16 TavernMap = 9;
	constexpr uint16 HouseMap = 11;
	constexpr int32 VillageMap = 1;
	/** The tavern's door to the village. */
	constexpr int32 Door = 19;
	constexpr int32 SettleFrames = 200;
	constexpr FPadState Nothing{};
	constexpr FPadState TowardDoor{static_cast<uint16>(FPadState::Bit(EPadButton::Down) | FPadState::Bit(EPadButton::Left))};
	constexpr FPadState AlongDoor{static_cast<uint16>(FPadState::Bit(EPadButton::Down) | FPadState::Bit(EPadButton::Right))};
	constexpr FPadState BesideDoor{FPadState::Bit(EPadButton::Down)};
	constexpr FPadState ConfirmAtDoor{static_cast<uint16>(TowardDoor.Buttons | FPadState::Bit(EPadButton::Cross))};
	/** In the house: down its wall away from the door, then at a run up the wall and into it, which slides along it. */
	constexpr FPadState DownTheWall{FPadState::Bit(EPadButton::Down)};
	constexpr FPadState RunIntoTheWall{static_cast<uint16>(FPadState::Bit(EPadButton::Up) | FPadState::Bit(EPadButton::Left) | FPadState::Bit(EPadButton::Circle))};
	constexpr int32 FramesDownTheWall = 16;
	/** A scratch variable no script of the tavern reaches, and a talk script that only counts in it: IncVar, End. */
	constexpr uint32 CountAddress = 0x436;
	constexpr std::array<uint8, 4> CountingScript{0x3C, 0x36, 0x04, 0x00};

	struct FVisit
	{
		std::unique_ptr<Testing::FGameLoopSession> Game;

		const FEventContext& Context() const { return Game->Loop->Field()->Runtime().Context(); }
		int32 MapLoads() const { return Game->Loop->Field()->Runtime().MapLoads(); }
		int32 Count() const { return Context().Variables.Raw(CountAddress); }

		void Run(FPadState Held, int32 Frames)
		{
			for (int32 Frame = 0; Frame < Frames; ++Frame)
			{
				const std::optional<FError> Stopped = Game->Frame(Held);
				INFO("frame " << Frame << ": " << (Stopped ? Stopped->Message : std::string()));
				REQUIRE_FALSE(Stopped.has_value());
				REQUIRE(Game->Loop->Field() != nullptr);
			}
		}

		/** Whether the party has left for the village within Frames more frames of Held. */
		bool LeavesWithin(FPadState Held, int32 Frames)
		{
			const int32 Loads = MapLoads();
			for (int32 Frame = 0; Frame < Frames && MapLoads() == Loads; ++Frame)
			{
				Run(Held, 1);
			}
			return MapLoads() != Loads && Context().MapNumber == VillageMap;
		}

		/** Makes the door's talk script count its starts and do nothing else. */
		void MakeDoorCount()
		{
			FEventScripts& Scripts = const_cast<FEventScripts&>(*Context().Scripts);
			const uint16 Entry = Scripts.EntryPoint(Door, FEventMachine::TalkScript);
			REQUIRE(Entry != 0);
			const std::array<uint8, 4> Was{Scripts.Code()[Entry], Scripts.Code()[Entry + 1], Scripts.Code()[Entry + 2], Scripts.Code()[Entry + 3]};
			REQUIRE(Scripts.Replace(Entry, Was, CountingScript));
		}
	};

	/** A new game's party on a map of Lahan, in the player's hands, doors opening when walked into or not. */
	FVisit Enter(const FIso9660& Disc, uint16 Map, bool bWalkIntoDoors)
	{
		FVisit Visit;
		Visit.Game = Testing::FGameLoopSession::Start(Disc);
		FPlayerSettings Settings = Visit.Game->Session->Settings();
		Settings.Controls.bWalkIntoDoors = bWalkIntoDoors;
		Visit.Game->Session->SetSettings(Settings);
		Visit.Game->EnterField(Map);
		Visit.Run(Nothing, SettleFrames);
		REQUIRE(Visit.Context().bWalkIntoDoors == bWalkIntoDoors);
		return Visit;
	}
}

TEST_CASE("A door opens when the party walks into it, where the setting lets doors", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FVisit Tavern = Enter(*Disc, TavernMap, true);
	// The walk to the door, a moment held up by it, the door's swing and the walk through it.
	CHECK(Tavern.LeavesWithin(TowardDoor, 60));
}

TEST_CASE("Without the setting a door opens for the confirm button alone", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FVisit Tavern = Enter(*Disc, TavernMap, false);
	CHECK_FALSE(Tavern.LeavesWithin(TowardDoor, 60));
	CHECK(Tavern.Context().Controls.PushedDoor == -1);
	// Held up at the door, facing it: the button opens it.
	Tavern.Run(ConfirmAtDoor, 1);
	CHECK(Tavern.LeavesWithin(TowardDoor, 30));
}

TEST_CASE("A door answers a walk into it once, and a walk along it not at all", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FVisit Tavern = Enter(*Disc, TavernMap, true);
	Tavern.MakeDoorCount();
	const int32 Before = Tavern.Count();

	// Nothing on the way to the door; once as the party is held up by it; no more however long it pushes on.
	Tavern.Run(TowardDoor, 8);
	CHECK(Tavern.Count() == Before);
	Tavern.Run(TowardDoor, 30);
	CHECK(Tavern.Count() == Before + 1);
	CHECK(Tavern.Context().Controls.PushedDoor == Door);
	Tavern.Run(TowardDoor, 60);
	CHECK(Tavern.Count() == Before + 1);

	// The button still asks, as often as it is pressed.
	Tavern.Run(ConfirmAtDoor, 1);
	Tavern.Run(TowardDoor, 4);
	CHECK(Tavern.Count() == Before + 2);

	// Let go and walked into again, it answers again.
	Tavern.Run(Nothing, 4);
	CHECK(Tavern.Context().Controls.PushedDoor == -1);
	Tavern.Run(TowardDoor, 10);
	CHECK(Tavern.Count() == Before + 3);

	// Walking along the door, the party does not walk at it: nothing.
	Tavern.Run(Nothing, 4);
	Tavern.Run(AlongDoor, 20);
	CHECK(Tavern.Count() == Before + 3);
}

TEST_CASE("A door opens for a walk that ends against it beside its middle", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FVisit Tavern = Enter(*Disc, TavernMap, true);
	// Part of the way, then straight down: the passage's wall turns the walk to the door, where it ends in the corner
	// with the door's middle more to the side than a talk's facing allows.
	Tavern.Run(TowardDoor, 10);
	CHECK(Tavern.LeavesWithin(BesideDoor, 40));
}

TEST_CASE("A door opens for a run that slides along its wall", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FVisit House = Enter(*Disc, HouseMap, true);
	House.Run(DownTheWall, FramesDownTheWall);
	// Up the wall and into it: the run never stops at the door, it slides past along the wall.
	CHECK(House.LeavesWithin(RunIntoTheWall, 30));
}

TEST_CASE("Without the setting a run along a door's wall opens nothing", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FVisit House = Enter(*Disc, HouseMap, false);
	House.Run(DownTheWall, FramesDownTheWall);
	CHECK_FALSE(House.LeavesWithin(RunIntoTheWall, 30));
}
