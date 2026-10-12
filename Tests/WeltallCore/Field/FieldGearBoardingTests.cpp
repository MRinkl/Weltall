#include "Field/EventMachine.h"
#include "Field/FieldGearBoarding.h"
#include "Game/GameState.h"
#include "Game/PsxRandom.h"
#include "Graphics/Sprite.h"
#include "Graphics/SpriteSystem.h"
#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

#include <memory>

using namespace Weltall;
using Weltall::Testing::FByteWriter;

// The party's gears boarded and left: the pilot and its gear hand each other their sprites, the parked
// gears' variables follow the member, and the scripts (FE 1F, FE 20) and the field (the buttons, the
// menu) board and leave through the same steps.

namespace
{
	constexpr int32 Pilot = 0;
	constexpr int32 Gear = 1;
	constexpr int32 SecondMember = 2;
	constexpr uint32 ParkedMap = 0x2A;
	constexpr int32 MapNumber = 0xC104;

	/** Three actors' sprites, which a swap hands from one actor to another. */
	class FSwappedSprites final : public IFieldActorModels
	{
	public:
		std::optional<FFieldSpriteResult> LoadSprite(const FFieldSpriteRequest&) override { return std::nullopt; }
		FSprite* ActorSprite(int32 Actor) override { return &Sprites[static_cast<std::size_t>(Index[static_cast<std::size_t>(Actor)])]; }
		FSpriteSystem* SpriteSystem() override { return &Kernel; }
		void SwapSprites(int32 First, int32 Second) override { std::swap(Index[static_cast<std::size_t>(First)], Index[static_cast<std::size_t>(Second)]); }
		bool LoadExtraAnimations(int32, int32) override { return false; }
		void UseExtraAnimations(int32) override {}

		std::array<FSprite, 3> Sprites;
		std::array<int32, 3> Index{0, 1, 2};
		FSpriteSystem Kernel;
	};

	/**
	 * A map in gear mode: slot 0's member (actor 0) and its parked gear (actor 1), slot 1's member (actor
	 * 2) without a gear actor, and one bytecode every actor's scripts start in.
	 */
	class FGearMap
	{
	public:
		explicit FGearMap(std::vector<uint8> Code = {0x00})
		{
			FByteWriter Writer;
			for (int32 Word = 0; Word < 32; ++Word)
			{
				Writer.U32(0);
			}
			Writer.U32(3);
			for (int32 Entry = 0; Entry < 3 * 32; ++Entry)
			{
				Writer.U16(0);
			}
			Writer.Bytes.insert(Writer.Bytes.end(), Code.begin(), Code.end());
			auto Parsed = FEventScripts::Parse(Writer.Bytes);
			REQUIRE(Parsed);
			Scripts = std::make_unique<FEventScripts>(std::move(*Parsed));
			Context.Scripts = Scripts.get();
			Context.MapNumber = MapNumber;
			Context.RunningMap = MapNumber & 0xFFF;
			Context.Random = &Random;
			Context.Models = &Models;
			Context.GameState = &Game;
			Context.PilotSpriteBase = 0x20;
			Context.PartyCharacters = {0, 2, FEventContext::NoActor};
			Context.PartyActors = {Pilot, SecondMember, FEventContext::NoActor};
			Context.GearActors = {Gear, FEventContext::NoActor, FEventContext::NoActor};
			for (int32 Actor = 0; Actor < 3; ++Actor)
			{
				FFieldActor Record;
				Record.Flags = FieldActorFlag::NoModel | FieldActorFlag::WalkingKind;
				Context.Actors.push_back(Record);
				FEventActorState& State = Context.States.emplace_back();
				State.Reset(Random.Next());
				State.Flags1 |= EventActorFlags1::NoAnimations;
			}
			FEventActorState& Member = State(Pilot);
			Member.Flags0 |= EventActorFlags0::PartyMember | EventActorFlags0::Airborne;
			Member.Layer = 1;
			Member.Position = {0x01234567, -0x00200000, -0x00501234};
			Member.FloorTriangles = {3, 4, 5, 6};
			Member.FloorNormal = {0, -4096, 0};
			Member.FloorHeight = -0x20;
			Member.TargetHeight = 7;
			Member.Facing = 0x400;
			Member.Yaw = 0x401;
			Member.IdleAnimation = 2;
			State(Gear).IdleAnimation = 5;
			State(Gear).Flags0 |= EventActorFlags0::PartyMember | EventActorFlags0::Frozen | EventActorFlags0::Falling;
			Context.Actors[Pilot].Matrix = {{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}};
			Context.Actors[Pilot].Translation = {0x123, -0x20, -0x502};
			Context.Actors[Gear].Flags |= FieldActorFlag::Hidden;
			Context.bInitDone = true;
		}

		FEventActorState& State(int32 Actor) { return Context.States[static_cast<std::size_t>(Actor)]; }
		int32 Var(uint32 Address) { return Context.Variables.Get(Address, *Scripts); }

		/** Runs the actor's script from pc 0 at once, as the machine runs a script set aside. */
		void Run(int32 Actor)
		{
			FEventThread& Thread = State(Actor).Threads[0];
			Thread.Pc = 0;
			Thread.Script = 0;
			Thread.SetPriority(0);
			for (int32 Other = 0; Other < 3; ++Other)
			{
				if (Other != Actor)
				{
					State(Other).Flags0 |= EventActorFlags0::Frozen;
				}
			}
			State(Actor).Flags0 &= ~EventActorFlags0::Frozen;
			const auto Error = FEventMachine::Update(Context);
			if (Error)
			{
				FAIL(Error->Message);
			}
		}

		std::unique_ptr<FEventScripts> Scripts;
		FPsxRandom Random;
		FSwappedSprites Models;
		FGameState Game;
		FEventContext Context;
	};
}

TEST_CASE("A member boarding its gear takes the gear's sprite where it stands and hides the gear", "[Field][Gears]")
{
	FGearMap Map;
	REQUIRE(BoardFieldGear(Map.Context, 0));
	CHECK(Map.Game.PartySlotInGear(0) == 1);
	// The pilot shows the gear's sprite, placed where the pilot stands.
	CHECK(Map.Models.ActorSprite(Pilot) == &Map.Models.Sprites[Gear]);
	CHECK(Map.Models.ActorSprite(Gear) == &Map.Models.Sprites[Pilot]);
	CHECK(Map.Models.Sprites[Gear].Position == Map.State(Pilot).Position);
	CHECK((Map.Context.Actors[Gear].Flags & FieldActorFlag::Hidden) != 0);
	const uint32 Pilots = Map.State(Pilot).Flags0;
	CHECK((Pilots & EventActorFlags0::GearPilot) != 0);
	CHECK((Pilots & (EventActorFlags0::PartyMember | EventActorFlags0::MapSprite | EventActorFlags0::InTheAir)) == 0);
	CHECK((Map.State(Gear).Flags0 & EventActorFlags0::InTheAir) == 0);
	CHECK(Map.State(Pilot).Animation == 2);
	CHECK(Map.State(Gear).Animation == 5);
	// The gear is parked where its member stands: this map with the member's layer, x and z.
	CHECK(Map.Var(ParkedMap) == (0x104 | 1 << 14));
	CHECK(Map.Var(ParkedMap + 2) == 0x0123);
	CHECK(static_cast<int16>(Map.Var(ParkedMap + 4)) == -0x51);
}

TEST_CASE("A member leaving its gear stands the gear where it stands, facing its way", "[Field][Gears]")
{
	FGearMap Map;
	REQUIRE(BoardFieldGear(Map.Context, 0));
	FEventActorState& Member = Map.State(Pilot);
	Member.Position = {0x00400000, 0, 0x00800000};
	Member.Flags0 |= EventActorFlags0::Falling;
	REQUIRE(LeaveFieldGear(Map.Context, 0));
	CHECK(Map.Game.PartySlotInGear(0) == 0);
	CHECK(Map.Models.ActorSprite(Pilot) == &Map.Models.Sprites[Pilot]);
	CHECK(Map.Models.ActorSprite(Gear) == &Map.Models.Sprites[Gear]);
	const FEventActorState& Gears = Map.State(Gear);
	CHECK(Gears.Position == Member.Position);
	CHECK(Gears.Layer == 1);
	CHECK(Gears.FloorTriangles == Member.FloorTriangles);
	CHECK(Gears.FloorNormal == Member.FloorNormal);
	CHECK(Gears.FloorHeight == -0x20);
	CHECK(Gears.TargetHeight == 7);
	CHECK(Gears.Facing == 0x400);
	CHECK(Gears.Yaw == 0x401);
	CHECK((Gears.Flags0 & (EventActorFlags0::Frozen | EventActorFlags0::InTheAir)) == 0);
	CHECK(Map.Context.Actors[Gear].Matrix == Map.Context.Actors[Pilot].Matrix);
	CHECK(Map.Context.Actors[Gear].Translation == Map.Context.Actors[Pilot].Translation);
	CHECK((Map.Context.Actors[Gear].Flags & (FieldActorFlag::Hidden | FieldActorFlag::KindBits)) == FieldActorFlag::WalkingKind);
	CHECK(Map.Models.Sprites[Gear].Position == Member.Position);
	CHECK(Map.Models.Sprites[Pilot].Position == Member.Position);
	CHECK((Member.Flags0 & (EventActorFlags0::PartyMember | EventActorFlags0::GearPilot | EventActorFlags0::InTheAir)) == EventActorFlags0::PartyMember);
	CHECK(Map.Var(ParkedMap + 2) == 0x40);
	CHECK(Map.Var(ParkedMap + 4) == 0x80);
}

TEST_CASE("The scripts board a gear from the gear's actor (FE 1F) and leave it by party slot (FE 20)", "[Field][Gears]")
{
	// FE 1F; FE 20 00; end.
	FGearMap Map({0xFE, 0x1F, 0xFE, 0x20, 0x00, 0x00});
	REQUIRE(BoardFieldGear(Map.Context, 0));
	// A gear its member rides already is not boarded again; the leave then runs.
	Map.Run(Gear);
	CHECK(Map.Game.PartySlotInGear(0) == 0);
	CHECK(Map.Models.ActorSprite(Pilot) == &Map.Models.Sprites[Pilot]);
	CHECK(Map.State(Gear).Threads[0].Pc == 5);

	FGearMap Boarding({0xFE, 0x1F, 0x00});
	Boarding.Run(Gear);
	CHECK(Boarding.Game.PartySlotInGear(0) == 1);
	CHECK(Boarding.Models.ActorSprite(Pilot) == &Boarding.Models.Sprites[Gear]);

	// An actor that is no slot's gear boards nothing.
	FGearMap NoGear({0xFE, 0x1F, 0x00});
	NoGear.Run(SecondMember);
	CHECK(NoGear.Game.PartySlotInGear(0) == 0);
	CHECK(NoGear.State(SecondMember).Threads[0].Pc == 2);

	// A slot without a character is left alone; a slot past the third faults.
	FGearMap Empty({0xFE, 0x20, 0x02, 0x00});
	Empty.Game.SetPartySlotInGear(2, 1);
	Empty.Run(Pilot);
	CHECK(Empty.Game.PartySlotInGear(2) == 1);
	FGearMap Past({0xFE, 0x20, 0x03, 0x00});
	Past.State(Pilot).Threads[0].SetPriority(0);
	Past.State(Gear).Flags0 |= EventActorFlags0::Frozen;
	Past.State(SecondMember).Flags0 |= EventActorFlags0::Frozen;
	CHECK(FEventMachine::Update(Past.Context).has_value());
}

TEST_CASE("The field boards by the buttons and after the menu, and stops at a slot without a gear actor", "[Field][Gears]")
{
	FGearMap Map;
	// The buttons: a walking member boards, a riding one leaves.
	REQUIRE_FALSE(BoardOrLeaveFieldGears(Map.Context, {true, false, false}, true));
	CHECK(Map.Game.PartySlotInGear(0) == 1);
	REQUIRE_FALSE(BoardOrLeaveFieldGears(Map.Context, {true, false, false}, true));
	CHECK(Map.Game.PartySlotInGear(0) == 0);
	// After the menu, which set who rides: now riding boards.
	Map.Game.SetPartySlotInGear(0, 1);
	REQUIRE_FALSE(BoardOrLeaveFieldGears(Map.Context, {true, false, false}, false));
	CHECK(Map.Models.ActorSprite(Pilot) == &Map.Models.Sprites[Gear]);
	// Outside gear mode nothing is boarded.
	Map.Context.PilotSpriteBase = 0;
	REQUIRE_FALSE(BoardOrLeaveFieldGears(Map.Context, {true, true, false}, true));
	CHECK(Map.Game.PartySlotInGear(0) == 1);
	// Slot 1's member has no gear actor to board.
	Map.Context.PilotSpriteBase = 0x20;
	const auto Error = BoardOrLeaveFieldGears(Map.Context, {false, true, false}, true);
	REQUIRE(Error);
	CHECK(Error->Message.find("party slot 1") != std::string::npos);
	CHECK(Map.Game.PartySlotInGear(1) == 0);
	CHECK(Map.Context.Run.Fault == EEventFault::None);
}
