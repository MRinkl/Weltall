#include "Field/EventMachine.h"
#include "Field/FieldWalkmesh.h"
#include "Game/GameState.h"
#include "Game/PsxRandom.h"
#include "Graphics/Sprite.h"
#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

#include <memory>

using namespace Weltall;
using Weltall::Testing::FByteWriter;

// A party slot's gear as a map's init places it (5C) from the slot's saved variables: the map it was last
// recorded on with the walkmesh layer in bits 14 and 15, then x and z.

namespace
{
	constexpr int32 Member = 0;
	constexpr int32 Gear = 1;
	constexpr uint32 ParkedMap = 0x2A;
	constexpr int32 MapNumber = 0xC042;

	/** A sprite of its own for each actor, made by nothing. */
	class FStandInSprites final : public IFieldActorModels
	{
	public:
		std::optional<FFieldSpriteResult> LoadSprite(const FFieldSpriteRequest&) override { return FFieldSpriteResult{0x30, false}; }
		FSprite* ActorSprite(int32 Actor) override { return &Sprites[static_cast<std::size_t>(Actor)]; }
		FSpriteSystem* SpriteSystem() override { return nullptr; }
		void SwapSprites(int32, int32) override {}
		bool LoadExtraAnimations(int32, int32) override { return true; }
		void UseExtraAnimations(int32) override {}

		std::array<FSprite, 2> Sprites;
	};

	/** A square of floor 40 units up over -1000..1000 on the one layer the actors walk, and the camera's layer. */
	FFieldWalkmesh OneFloor()
	{
		FWalkmeshLayer Layer;
		Layer.Vertices = {FGteVector{-1000, -40, -1000}, FGteVector{1000, -40, -1000}, FGteVector{1000, -40, 1000}, FGteVector{-1000, -40, 1000}};
		FWalkmeshTriangle First;
		First.Vertices = {0, 2, 1};
		First.Neighbours = {-1, -1, -1};
		FWalkmeshTriangle Second;
		Second.Vertices = {0, 3, 2};
		Second.Neighbours = {-1, -1, -1};
		Layer.Triangles = {First, Second};
		FFieldWalkmesh Walkmesh;
		Walkmesh.Layers = {Layer, Layer};
		Walkmesh.Attributes = {0};
		return Walkmesh;
	}

	/** A map in gears: slot 0's member (actor 0) and the actor its init makes the slot's gear (actor 1) by 5C. */
	class FParkedGearMap
	{
	public:
		FParkedGearMap() : Walkmesh(OneFloor())
		{
			FByteWriter Writer;
			for (int32 Word = 0; Word < 32; ++Word)
			{
				Writer.U32(0);
			}
			Writer.U32(2);
			for (int32 Entry = 0; Entry < 2 * 32; ++Entry)
			{
				Writer.U16(0);
			}
			// 5C with slot 0 as a constant, then the script's end.
			Writer.Bytes.insert(Writer.Bytes.end(), {0x5C, 0x00, 0x80, 0x00});
			auto Parsed = FEventScripts::Parse(Writer.Bytes);
			REQUIRE(Parsed);
			Scripts = std::make_unique<FEventScripts>(std::move(*Parsed));
			Context.Scripts = Scripts.get();
			Context.MapNumber = MapNumber;
			Context.RunningMap = MapNumber & 0xFFF;
			Context.Random = &Random;
			Context.Models = &Models;
			Context.GameState = &Game;
			Context.Walkmesh = &Walkmesh;
			Context.PartyCharacters = {0, FEventContext::NoActor, FEventContext::NoActor};
			Context.PartyActors = {Member, FEventContext::NoActor, FEventContext::NoActor};
			for (int32 Actor = 0; Actor < 2; ++Actor)
			{
				FFieldActor Record;
				Record.Flags = FieldActorFlag::NoModel | FieldActorFlag::WalkingKind;
				Context.Actors.push_back(Record);
				Context.States.emplace_back().Reset(Random.Next());
			}
			State(Member).Flags0 |= EventActorFlags0::Frozen;
			REQUIRE(Game.SetCharacterGear(0, 0));
		}

		FEventActorState& State(int32 Actor) { return Context.States[static_cast<std::size_t>(Actor)]; }

		/** The slot's gear recorded on Map (its number and layer) at (X, Z). */
		void Record(int32 Map, int32 X, int32 Z)
		{
			Context.Variables.Set(ParkedMap, Map);
			Context.Variables.Set(ParkedMap + 2, X);
			Context.Variables.Set(ParkedMap + 4, Z);
		}

		/** Runs the gear actor's script from its start; what stopped it, if anything did. */
		std::optional<FError> Place()
		{
			FEventThread& Thread = State(Gear).Threads[0];
			Thread.Pc = 0;
			Thread.Script = 0;
			Thread.SetPriority(0);
			return FEventMachine::Update(Context);
		}

		std::unique_ptr<FEventScripts> Scripts;
		FPsxRandom Random;
		FStandInSprites Models;
		FGameState Game;
		FFieldWalkmesh Walkmesh;
		FEventContext Context;
	};
}

TEST_CASE("A gear parked on the map stands on the floor where it was left", "[Field][Gears]")
{
	FParkedGearMap Map;
	Map.Record(0x42, 300, -200);
	REQUIRE_FALSE(Map.Place());
	const FEventActorState& Parked = Map.State(Gear);
	CHECK(Parked.Position == std::array<int32, 3>{300 << 16, -40 * 0x10000, -200 * 0x10000});
	CHECK(Parked.Layer == 0);
	CHECK(Parked.FloorHeight == -40);
	CHECK(Parked.FloorNormal != std::array<int32, 3>{});
	CHECK((Map.Context.Actors[Gear].Flags & FieldActorFlag::Hidden) == 0);
	CHECK(Map.Context.GearActors[0] == Gear);
}

TEST_CASE("A gear last recorded on another map waits hidden at the map's origin", "[Field][Gears]")
{
	FParkedGearMap Map;
	Map.Record(0x45 | 2 << 14, 300, -200);
	REQUIRE_FALSE(Map.Place());
	const FEventActorState& Parked = Map.State(Gear);
	CHECK(Parked.Position == std::array<int32, 3>{0, -40 * 0x10000, 0});
	CHECK(Parked.Layer == 0);
	CHECK((Map.Context.Actors[Gear].Flags & FieldActorFlag::Hidden) != 0);
}

TEST_CASE("A gear ridden in from a higher layer than the map has is placed without a floor, on the first layer", "[Field][Gears]")
{
	// The member rode its gear on the third layer of the map before; the frames of the map change recorded this
	// map's number beside that layer and the place it rode at. The map has one layer to walk on.
	FParkedGearMap Map;
	Map.Game.SetPartySlotInGear(0, 1);
	Map.Record(0x42 | 2 << 14, -250, 934);
	const std::optional<FError> Error = Map.Place();
	INFO((Error ? Error->Message : std::string()));
	REQUIRE_FALSE(Error);
	const FEventActorState& Parked = Map.State(Gear);
	// Where it was recorded, at height 0 with no floor normal, as an actor on the camera's layer stands.
	CHECK(Parked.Position == std::array<int32, 3>{-250 * 0x10000, 0, 934 << 16});
	CHECK(Parked.FloorHeight == 0);
	CHECK(Parked.FloorNormal == std::array<int32, 3>{});
	CHECK(Parked.FloorAttributes == 0);
	CHECK(Parked.Layer == 0);
	// Its member rides it: the gear's own actor is not shown.
	CHECK((Map.Context.Actors[Gear].Flags & FieldActorFlag::Hidden) != 0);
	// The fourth layer likewise.
	FParkedGearMap Fourth;
	Fourth.Record(0x42 | 3 << 14, 0, 0);
	REQUIRE_FALSE(Fourth.Place());
	CHECK(Fourth.State(Gear).Layer == 0);
	CHECK(Fourth.State(Gear).FloorHeight == 0);
}
