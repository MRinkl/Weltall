#include "Game/FieldProgram.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "Support/PortStory.h"

#include <catch_amalgamated.hpp>

#include <cmath>
#include <optional>
#include <string>

using namespace Weltall;

namespace
{
	constexpr int32 ForestMap = 22;
	constexpr int32 Frames = 60;

	/** Where the leader stands after walking up the forest with Held, the party running by itself or not. */
	FFieldPoint LeaderAfter(const FIso9660& Disc, bool bAlwaysRun, bool bRunButton)
	{
		auto Game = Testing::FGameLoopSession::Start(Disc);
		FPlayerSettings Settings = Game->Session->Settings();
		Settings.Controls.bAlwaysRun = bAlwaysRun;
		Game->Session->SetSettings(Settings);
		Game->EnterField(ForestMap);
		FPadState Held{FPadState::Bit(EPadButton::Up)};
		if (bRunButton)
		{
			Held.Buttons |= FPadState::Bit(EPadButton::Circle);
		}
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			const std::optional<FError> Stopped = Game->Frame(Held);
			INFO("frame " << Frame << ": " << (Stopped ? Stopped->Message : std::string()));
			REQUIRE_FALSE(Stopped.has_value());
		}
		REQUIRE(Game->Loop->Field() != nullptr);
		const FEventContext& Context = Game->Loop->Field()->Runtime().Context();
		return Context.States[Context.LeaderActor].Position;
	}
}

TEST_CASE("With always run the party runs by itself and the run button walks", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FFieldPoint Walked = LeaderAfter(*Disc, false, false);
	const FFieldPoint Ran = LeaderAfter(*Disc, false, true);
	REQUIRE(Walked != Ran);
	CHECK(LeaderAfter(*Disc, true, false) == Ran);
	CHECK(LeaderAfter(*Disc, true, true) == Walked);
}

TEST_CASE("With always run Bart swims fast in Aveh's waterway and the run button slows him", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// The waterway on the tournament's day, Bart alone by the floodgate. The map's own script gives his swim its fast
	// stroke while the run button is held, read as the leader's run under always run.
	constexpr int32 Waterway = 130;
	constexpr uint16 StoryProgress = 0;
	constexpr uint16 InTheWaterway = 70;
	constexpr uint16 ByTheFloodgate = 4;
	constexpr int32 StartFrames = 10;
	/** About the fast stroke's way in a frame, in units. */
	constexpr double OneFrame = 12.0;
	const auto Swum = [&](bool bAlwaysRun, bool bRunButton)
	{
		auto Game = Testing::FGameLoopSession::Start(*Disc);
		FPlayerSettings Settings = Game->Session->Settings();
		Settings.Controls.bAlwaysRun = bAlwaysRun;
		Game->Session->SetSettings(Settings);
		FGameState& State = Game->Session->Game();
		State.SetSavedEventVariable(StoryProgress, InTheWaterway);
		State.SetParty({3, 0xFF, 0xFF});
		State.SetFieldEntrance(ByTheFloodgate);
		Game->EnterField(Waterway);
		// The run button is held from the start, as always run holds it: the script picks the stroke as Bart floats too.
		const FPadState Run{bRunButton ? FPadState::Bit(EPadButton::Circle) : uint16{0}};
		const auto Controlled = [&]() { return Game->Loop->Field() != nullptr && Game->Loop->Field()->Runtime().Context().bPlayerControlled; };
		REQUIRE(Game->RunUntil(Controlled, 600, Run) < 600);
		const FEventContext& Context = Game->Loop->Field()->Runtime().Context();
		const FPadState Held{static_cast<uint16>(FPadState::Bit(EPadButton::Down) | Run.Buttons)};
		// Measured once the swim is under way: as the party is given back, a held button is read a tick before the run
		// always run stands for, which waits for the player to have the party.
		for (int32 Frame = 0; Frame < StartFrames; ++Frame)
		{
			REQUIRE_FALSE(Game->Frame(Held).has_value());
		}
		const FFieldPoint From = Context.States[Context.LeaderActor].Position;
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			REQUIRE_FALSE(Game->Frame(Held).has_value());
		}
		const FFieldPoint To = Context.States[Context.LeaderActor].Position;
		return std::hypot(static_cast<double>((To[0] >> 16) - (From[0] >> 16)), static_cast<double>((To[2] >> 16) - (From[2] >> 16)));
	};
	const double Slow = Swum(false, false);
	const double Fast = Swum(false, true);
	REQUIRE(Fast > Slow + OneFrame);
	// Within a frame of the original's own: the head start leaves Bart where the current pushes him a little otherwise.
	CHECK(std::abs(Swum(true, false) - Fast) <= OneFrame);
	CHECK(std::abs(Swum(true, true) - Slow) <= OneFrame);
}

TEST_CASE("With always run Bart swims off the spot by the waterway's way out at the fast stroke", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// The waterway on the tournament's day, Bart alone on the spot by its way to map 125 (entrance 3): the water lies
	// to the west, the ramp up to the way out to the east. On the spot the map's own script gives him the walking speed,
	// and back in the water it gives the fast stroke as the run button comes to be held, which under always run it is
	// all the while; the fix forgets the stroke on the spot (Content/Story/additions/waterway-swim-stroke.txt).
	constexpr int32 Waterway = 130;
	constexpr uint16 StoryProgress = 0;
	constexpr uint16 InTheWaterway = 70;
	constexpr uint16 ByTheWayOut = 3;
	constexpr uint8 TheSpot = 118;
	constexpr uint16 FastStroke = 128;
	constexpr uint16 WalkingSpeed = 256;
	constexpr int32 SwimFrames = 16;
	constexpr int32 MostFrames = 40;
	const auto SpeedOffTheSpot = [&](const FEventOptions& Fixes)
	{
		auto Game = Testing::FGameLoopSession::Start(*Disc);
		Game->Session->SetStory(&Testing::PortStory());
		FPlayerSettings Settings = Game->Session->Settings();
		Settings.Controls.bAlwaysRun = true;
		Settings.Fixes.Field = Fixes;
		Game->Session->SetSettings(Settings);
		FGameState& State = Game->Session->Game();
		State.SetSavedEventVariable(StoryProgress, InTheWaterway);
		State.SetParty({3, 0xFF, 0xFF});
		State.SetFieldEntrance(ByTheWayOut);
		Game->EnterField(Waterway);
		const auto Controlled = [&]() { return Game->Loop->Field() != nullptr && Game->Loop->Field()->Runtime().Context().bPlayerControlled; };
		REQUIRE(Game->RunUntil(Controlled, MostFrames * 15) < MostFrames * 15);
		const FEventContext& Context = Game->Loop->Field()->Runtime().Context();
		const auto Leader = [&]() -> const FEventActorState& { return Context.States[Context.LeaderActor]; };
		const auto OnTheSpot = [&]()
		{
			const std::size_t Layer = static_cast<std::size_t>(static_cast<int16>(Leader().Layer));
			const std::size_t Triangle = static_cast<std::size_t>(Leader().FloorTriangles[Layer]);
			return Context.Walkmesh->Layers[Layer].Triangles[Triangle].Attribute == TheSpot;
		};
		const auto SwimWest = [&]()
		{
			for (int32 Frame = 0; Frame < SwimFrames; ++Frame)
			{
				REQUIRE_FALSE(Game->Frame(FPadState{FPadState::Bit(EPadButton::Left)}).has_value());
			}
		};
		SwimWest();
		REQUIRE(Leader().SpeedDivisor == FastStroke);
		REQUIRE(Game->RunUntil(OnTheSpot, MostFrames, FPadState{FPadState::Bit(EPadButton::Right)}) < MostFrames);
		REQUIRE(Game->RunUntil([&]() { return Leader().SpeedDivisor == WalkingSpeed; }, MostFrames) < MostFrames);
		REQUIRE(OnTheSpot());
		SwimWest();
		return Leader().SpeedDivisor;
	};
	CHECK(SpeedOffTheSpot(FEventOptions()) == FastStroke);
	// The original's stroke, chosen only as the button changes, stays at the walking speed the spot gave.
	CHECK(SpeedOffTheSpot(FEventOptions::Original()) == WalkingSpeed);
}
