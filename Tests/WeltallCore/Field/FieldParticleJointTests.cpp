#include "Field/EventContext.h"
#include "Field/FieldParticles.h"
#include "Game/PsxRandom.h"
#include "Gears/GearModels.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <memory>

using namespace Weltall;

namespace
{
	constexpr int32 Lifetime = 3;

	/** A map of one actor with the gears' module and no gear in it, and an effect of one particle born in its first tick. */
	struct FOneParticle
	{
		std::unique_ptr<FEventContext> Context = std::make_unique<FEventContext>();
		FPsxRandom Random{1};

		explicit FOneParticle(EFieldParticleSpace Space)
		{
			Context->States.emplace_back();
			Context->Actors.emplace_back();
			Context->Random = &Random;
			Context->Gears.Models = std::make_unique<FGearModels>();
			REQUIRE_FALSE(Context->Gears.Models->Init(FFieldGears::DefaultPoolSize));
			Context->Particles.ResetSetup(0);
			FFieldParticleEmitter& Emitter = Context->Particles.Setup[0];
			Emitter.Count = 1;
			Emitter.EmitTicks = 1;
			Emitter.Interval = 1;
			Emitter.Lifetime = Lifetime;
			Emitter.Origin = {10, -20, 30};
			Emitter.Target = {10, -120, 30};
			Emitter.Speed = 0x1000;
			Emitter.SpawnRadius = 0;
			Emitter.TargetSpread = 0;
			Emitter.FacingOffsets = {};
			Emitter.Flags = static_cast<uint16>(static_cast<uint16>(Space) << FieldParticleFlags::SpaceShift);
			Emitter.GearSlot = 1;
			Emitter.GearJoint = 31;
			REQUIRE(Context->Particles.Start(0));
		}

		const FFieldParticle& Particle() const { return Context->Particles.Effects[0].Particles[0][0]; }
	};
}

TEST_CASE("A particle due while its gear's slot holds no model is placed about the map's origin and never drawn", "[Field][Gears]")
{
	FOneParticle Effect(EFieldParticleSpace::GearJoint);
	// Born and placed in the one tick: by no joint, so its start counts from the origin, as the emitter's own does.
	REQUIRE_FALSE(UpdateFieldParticles(*Effect.Context));
	REQUIRE(Effect.Particle().bAlive);
	CHECK(Effect.Particle().bJointless);
	CHECK(Effect.Particle().Position == std::array<int32, 3>{20 * 4096, -40 * 4096, 60 * 4096});
	// Unturned: it heads straight up, as its emitter aims it.
	CHECK(Effect.Particle().Velocity[0] == 0);
	CHECK(Effect.Particle().Velocity[1] < 0);
	CHECK(Effect.Particle().Velocity[2] == 0);
	// It lives its ticks unseen, and the effect ends as any other does.
	for (int32 Tick = 0; Tick < Lifetime; ++Tick)
	{
		REQUIRE(Effect.Particle().bAlive);
		REQUIRE_FALSE(UpdateFieldParticles(*Effect.Context));
		CHECK_FALSE(Effect.Particle().bDrawn);
	}
	CHECK_FALSE(Effect.Particle().bAlive);
	REQUIRE_FALSE(UpdateFieldParticles(*Effect.Context));
	CHECK_FALSE(Effect.Context->Particles.Effects[0].bActive);
}

TEST_CASE("A particle placed around its owner is drawn from the tick after, and a new life starts with a joint to find", "[Field]")
{
	FOneParticle Effect(EFieldParticleSpace::Owner);
	REQUIRE_FALSE(UpdateFieldParticles(*Effect.Context));
	CHECK_FALSE(Effect.Particle().bJointless);
	CHECK_FALSE(Effect.Particle().bDrawn);
	REQUIRE_FALSE(UpdateFieldParticles(*Effect.Context));
	CHECK(Effect.Particle().bDrawn);

	// A particle that was once without its joint is not held to it when the emitter gives it another life.
	FOneParticle Again(EFieldParticleSpace::GearJoint);
	Again.Context->Particles.Effects[0].Emitters[0].EmitTicks = Lifetime + 2;
	for (int32 Tick = 0; Tick <= Lifetime; ++Tick)
	{
		REQUIRE_FALSE(UpdateFieldParticles(*Again.Context));
	}
	REQUIRE_FALSE(Again.Particle().bAlive);
	Again.Context->Particles.Effects[0].Emitters[0].Flags = 0;
	REQUIRE_FALSE(UpdateFieldParticles(*Again.Context));
	REQUIRE(Again.Particle().bAlive);
	CHECK_FALSE(Again.Particle().bJointless);
}
