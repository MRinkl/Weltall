#include "Field/EventMachine.h"
#include "Field/FieldModels.h"
#include "Game/PsxRandom.h"
#include "Gears/GearModels.h"
#include "Graphics/GteVectors.h"
#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <memory>

using namespace Weltall;
using Weltall::Testing::FByteWriter;

namespace
{
	constexpr std::array<std::array<int16, 3>, 3> Identity = FGteMatrix::Identity().M;

	FFieldActor Model(int32 X, int32 Y, int32 Z)
	{
		FFieldActor Record;
		Record.Matrix = Identity;
		Record.Translation = {X, Y, Z};
		return Record;
	}

	/** Two script actors and one piece of scenery, all models. */
	FEventContext TwoActorsAndScenery()
	{
		FEventContext Context;
		Context.Actors = {Model(10, 0, 0), Model(0, 5, 0), Model(100, 0, 100)};
		Context.States.resize(2);
		for (FEventActorState& State : Context.States)
		{
			State.Parent = 0xFF;
			State.GearJoint = 0xFFFF;
		}
		return Context;
	}

	class FAnySprite final : public IFieldActorModels
	{
	public:
		std::optional<FFieldSpriteResult> LoadSprite(const FFieldSpriteRequest&) override { return FFieldSpriteResult{0x30, false}; }
		FSprite* ActorSprite(int32) override { return nullptr; }
		FSpriteSystem* SpriteSystem() override { return nullptr; }
		void SwapSprites(int32, int32) override {}
		bool LoadExtraAnimations(int32, int32) override { return false; }
		void UseExtraAnimations(int32) override {}
	};
}

TEST_CASE("Every field actor's world placement is its own unless it is attached to another's model", "[Field]")
{
	FEventContext Context = TwoActorsAndScenery();
	Context.Actors[0].Matrix = {{{0, 0, 4096}, {0, 4096, 0}, {-4096, 0, 0}}};
	Context.States[1].Parent = 0;
	REQUIRE_FALSE(UpdateFieldModels(Context));
	CHECK(Context.Actors[0].WorldMatrix == Context.Actors[0].Matrix);
	CHECK(Context.Actors[0].Position == Context.Actors[0].Translation);
	// Actor 1 hangs off actor 0: turned by its matrix and moved to its place.
	const FGteMatrix Expected = CompMatrix(FGteMatrix{Context.Actors[0].Matrix, {10, 0, 0}}, FGteMatrix{Identity, {0, 5, 0}});
	CHECK(Context.Actors[1].WorldMatrix == Expected.M);
	CHECK(Context.Actors[1].Position == Expected.T);
	CHECK(Context.Actors[1].Translation == std::array<int32, 3>{0, 5, 0});
}

TEST_CASE("A parent later in the field's actors lends its world placement from the tick before", "[Field]")
{
	FEventContext Context = TwoActorsAndScenery();
	Context.States[0].Parent = 1;
	Context.Actors[1].WorldMatrix = Identity;
	Context.Actors[1].Position = {1000, 0, 0};
	REQUIRE_FALSE(UpdateFieldModels(Context));
	CHECK(Context.Actors[0].Position == std::array<int32, 3>{1010, 0, 0});
	CHECK(Context.Actors[1].Position == std::array<int32, 3>{0, 5, 0});
}

TEST_CASE("The scenery drifts by its step each tick, after its placement is taken", "[Field]")
{
	FEventContext SceneryOnly = TwoActorsAndScenery();
	SceneryOnly.Render.DriftStep = {2, -1, 3};
	REQUIRE_FALSE(UpdateFieldModels(SceneryOnly));
	CHECK(SceneryOnly.Actors[2].Position == std::array<int32, 3>{100, 0, 100});
	CHECK(SceneryOnly.Actors[2].Translation == std::array<int32, 3>{102, -1, 103});
	CHECK(SceneryOnly.Actors[0].Translation == std::array<int32, 3>{10, 0, 0});
	CHECK(SceneryOnly.Render.DriftTotal == std::array<int32, 3>{2, -1, 3});

	FEventContext Everything = TwoActorsAndScenery();
	Everything.Render.DriftStep = {2, -1, 3};
	Everything.Render.SceneryDrift = 1;
	Everything.States[1].Parent = 0;
	REQUIRE_FALSE(UpdateFieldModels(Everything));
	CHECK(Everything.Actors[0].Translation == std::array<int32, 3>{12, -1, 3});
	CHECK(Everything.Actors[1].Translation == std::array<int32, 3>{0, 5, 0});
	CHECK(Everything.Actors[2].Translation == std::array<int32, 3>{102, -1, 103});

	FEventContext Still = TwoActorsAndScenery();
	Still.Render.DriftStep = {2, -1, 3};
	Still.Render.SceneryDrift = 2;
	REQUIRE_FALSE(UpdateFieldModels(Still));
	CHECK(Still.Actors[2].Translation == std::array<int32, 3>{100, 0, 100});
}

TEST_CASE("A model riding a gear's joint stops the models' update on a map whose load loaded no gears", "[Field]")
{
	FEventContext Context = TwoActorsAndScenery();
	Context.States[1].GearJoint = 0x1002;
	CHECK(UpdateFieldModels(Context).has_value());
}

TEST_CASE("A model riding the joint of an empty gear slot is placed by its own placement twice and does not drift", "[Field]")
{
	FEventContext Context = TwoActorsAndScenery();
	Context.Gears.Models = std::make_unique<FGearModels>();
	REQUIRE_FALSE(Context.Gears.Models->Init(FFieldGears::DefaultPoolSize));
	Context.Actors[1].Matrix = {{{0, 0, 4096}, {0, 4096, 0}, {-4096, 0, 0}}};
	Context.States[1].GearJoint = 0x1002;
	Context.Render.DriftStep = {2, -1, 3};
	Context.Render.SceneryDrift = 1;
	REQUIRE_FALSE(UpdateFieldModels(Context));
	const FGteMatrix Own{Context.Actors[1].Matrix, {0, 5, 0}};
	const FGteMatrix Expected = CompMatrix(Own, Own);
	CHECK(Context.Actors[1].WorldMatrix == Expected.M);
	CHECK(Context.Actors[1].Position == Expected.T);
	CHECK(Context.Actors[1].Translation == std::array<int32, 3>{0, 5, 0});
}

TEST_CASE("FE 1D sets the scenery's drift step and lets the drifting models escape the culling", "[Field]")
{
	FByteWriter Writer;
	for (int32 Word = 0; Word < 32; ++Word)
	{
		Writer.U32(0);
	}
	Writer.U32(1);
	for (int32 Entry = 0; Entry < 32; ++Entry)
	{
		Writer.U16(0);
	}
	// FE 1D x 3, z -2, y 1, all constants; then the end of the script.
	for (const uint8 Byte : std::to_array<uint8>({0xFE, 0x1D, 0x03, 0x00, 0xFE, 0xFF, 0x01, 0x00, 0xE0, 0x00}))
	{
		Writer.U8(Byte);
	}
	auto Scripts = FEventScripts::Parse(Writer.Bytes);
	REQUIRE(Scripts);
	FPsxRandom Random(1);
	FAnySprite Models;
	FEventContext Context;
	Context.Scripts = &*Scripts;
	Context.Random = &Random;
	Context.Models = &Models;
	Context.Actors = {Model(0, 0, 0)};
	Context.States.resize(1);
	Context.States[0].Reset(0);
	REQUIRE_FALSE(FEventMachine::InitMap(Context));
	CHECK(Context.Render.DriftStep == std::array<int16, 3>{3, 1, -2});
	CHECK(Context.Render.SceneryDrift == FieldSceneryDrift::DrawnOffScreen);
}
