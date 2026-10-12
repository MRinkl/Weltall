#include "Field/EventMachine.h"
#include "Field/FieldWalkmesh.h"
#include "Game/PsxRandom.h"
#include "Graphics/Sprite.h"
#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

#include <memory>

using namespace Weltall;
using Weltall::Testing::FByteWriter;

namespace
{
	/** One sprite of its own for each actor, made by nothing. */
	class FStandInModels final : public IFieldActorModels
	{
	public:
		explicit FStandInModels(std::size_t Actors) : Sprites(Actors) {}

		std::optional<FFieldSpriteResult> LoadSprite(const FFieldSpriteRequest&) override { return FFieldSpriteResult{0x30, false}; }
		FSprite* ActorSprite(int32 Actor) override { return &Sprites[static_cast<std::size_t>(Actor)]; }
		FSpriteSystem* SpriteSystem() override { return nullptr; }
		void SwapSprites(int32, int32) override {}
		bool LoadExtraAnimations(int32, int32) override { return true; }
		void UseExtraAnimations(int32) override {}

		std::vector<FSprite> Sprites;
	};

	/** A square of floor at height 0 over -100..100, a layer to walk on and the camera's. */
	FFieldWalkmesh SquareWalkmesh()
	{
		FWalkmeshLayer Layer;
		Layer.Vertices = {FGteVector{-100, 0, -100}, FGteVector{100, 0, -100}, FGteVector{100, 0, 100}, FGteVector{-100, 0, 100}};
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

	/** One actor whose default script is the code, standing at the origin of a square of floor. */
	class FOneActorMap
	{
	public:
		explicit FOneActorMap(std::vector<uint8> Code) : Models(1), Walkmesh(SquareWalkmesh())
		{
			FByteWriter Writer;
			for (int32 Word = 0; Word < 32; ++Word)
			{
				Writer.U32(0);
			}
			Writer.U32(1);
			for (int32 Script = 0; Script < 32; ++Script)
			{
				Writer.U16(0);
			}
			Writer.Bytes.insert(Writer.Bytes.end(), Code.begin(), Code.end());
			auto Parsed = FEventScripts::Parse(Writer.Bytes);
			REQUIRE(Parsed);
			Scripts = std::make_unique<FEventScripts>(std::move(*Parsed));
			Context.Scripts = Scripts.get();
			Context.Random = &Random;
			Context.Models = &Models;
			Context.Walkmesh = &Walkmesh;
			FFieldActor Record;
			Record.Flags = 0x200;
			Context.Actors.push_back(Record);
			Context.States.emplace_back().Reset(Random.Next());
			Context.bInitDone = true;
		}

		FEventActorState& State() { return Context.States[0]; }
		FSprite& Sprite() { return Models.Sprites[0]; }

		std::optional<FError> Tick() { return FEventMachine::Update(Context); }

		std::unique_ptr<FEventScripts> Scripts;
		FPsxRandom Random;
		FStandInModels Models;
		FFieldWalkmesh Walkmesh;
		FEventContext Context;
	};
}

TEST_CASE("A jump flies its ticks under the sprite's gravity and then lands on its point", "[Field]")
{
	// 57 00: to (10, -16, 20) over 4 ticks, the operands constants; 57 83 its flight; then 00.
	FOneActorMap Map({0x57, 0x00, 0x0A, 0x00, 0x14, 0x00, 0xF0, 0xFF, 0x04, 0x00, 0xF0, 0x57, 0x83, 0x00});
	Map.Sprite().Gravity = 0x4000;
	FEventActorState& State = Map.State();
	// A locked facing keeps the flight from turning the actor, which needs libgte's arctangents.
	State.Flags0 |= EventActorFlags0::FacingLocked;
	REQUIRE_FALSE(Map.Tick());
	CHECK(State.JumpTicks == 4);
	CHECK(State.Pc == 11);
	// The horizontal step covers the distance in one tick more than the jump; the vertical speed reaches
	// the height in the jump's ticks against the gravity.
	CHECK(State.MoveWork == std::array<int32, 3>{0x20000, 0, 0x40000});
	CHECK(Map.Sprite().Velocity[1] == -0x8000 - 0x40000);
	for (int32 Tick = 0; Tick < 4; ++Tick)
	{
		REQUIRE_FALSE(Map.Tick());
		CHECK(State.Pc == 11);
	}
	CHECK(State.Position == std::array<int32, 3>{0x80000, -0x100000 - 0x8000 * 4 + 0x4000 * 6, 0x100000});
	CHECK((State.Flags0 & EventActorFlags0::MovedByScript) != 0);
	REQUIRE_FALSE(Map.Tick());
	CHECK(State.Position == std::array<int32, 3>{0xA0000, -0x100000, 0x140000});
	CHECK(State.Pc == 13);
	CHECK((State.Flags0 & EventActorFlags0::MovedByScript) == 0);
	CHECK(Map.Context.Actors[0].Translation == std::array<int32, 3>{10, -16, 20});
	CHECK(Map.Sprite().Position == State.Position);
}

TEST_CASE("Movement bounds are kept until FE 16 frees them", "[Field]")
{
	// 17 with every corner constant, then FE 16.
	FOneActorMap Map({0x17, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0xF8, 0xFF, 0xFF, 0xC3, 0xFE, 0x16, 0xC3});
	REQUIRE_FALSE(Map.Tick());
	CHECK((Map.State().Status & 0x1000) != 0);
	CHECK(Map.State().MoveBounds == std::array<int16, 8>{1, 2, 3, 4, 5, 6, 7, -8});
	REQUIRE_FALSE(Map.Tick());
	CHECK((Map.State().Status & 0x1000) == 0);
}

TEST_CASE("An actor placed on the camera's layer stands at height 0 with no floor normal; past the layers it is refused", "[Field]")
{
	// 1A 01: the second of two layers is the camera's, which has no floor for actors.
	FOneActorMap Camera({0x1A, 0x01, 0x00});
	REQUIRE_FALSE(Camera.Tick());
	CHECK(Camera.State().FloorHeight == 0);
	CHECK(Camera.State().FloorNormal == std::array<int32, 3>{});
	// On the layer it walks on it stands on the floor, whose normal points up.
	FOneActorMap Walking({0x1A, 0x00, 0x00});
	REQUIRE_FALSE(Walking.Tick());
	CHECK(Walking.State().FloorHeight == 0);
	CHECK(Walking.State().FloorNormal != std::array<int32, 3>{});
	// 1A 02: a layer the map does not have, whose floor the original takes from stale stack.
	FOneActorMap Missing({0x1A, 0x02, 0x00});
	const std::optional<FError> Error = Missing.Tick();
	REQUIRE(Error);
	CHECK(Error->Message.find("which the map does not have") != std::string::npos);
}
