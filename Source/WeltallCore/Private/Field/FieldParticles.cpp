#include "Field/FieldParticles.h"

namespace Weltall
{
	void FFieldParticles::ResetSetup(int16 Actor)
	{
		for (FFieldParticleEmitter& Emitter : Setup)
		{
			// The words the reset leaves alone keep what they held.
			Emitter.Owner = Actor;
			Emitter.Word00 = 0;
			Emitter.Delay = 0;
			Emitter.EmitTicks = 0x80;
			Emitter.Count = 0;
			Emitter.Origin = {};
			Emitter.Target = {0, -1000, 0};
			Emitter.Speed = 0x8000;
			Emitter.Scale = 0x800;
			Emitter.SpeedScale = 1;
			Emitter.Acceleration = {};
			Emitter.TargetSpread = 0x100;
			Emitter.Lifetime = 0x1C;
			Emitter.SpawnRadius = 0;
			Emitter.Flags = 0;
			Emitter.Roll = 0;
			Emitter.Interval = 1;
			Emitter.Sprite = 0;
			Emitter.Size = {0x1C8, 0x1C8, 0x1C8};
			Emitter.SizeStep = {0x20, 0x20, 0x20};
			Emitter.Color = {0x80, 0x20, 0};
			Emitter.ColorStep = {-4, -1, 0};
			Emitter.FacingOffsets = {};
		}
	}

	bool FFieldParticles::Start(int16 Actor)
	{
		for (FFieldParticleEffect& Effect : Effects)
		{
			if (Effect.bActive)
			{
				continue;
			}
			Effect.bActive = true;
			Effect.Actor = Actor;
			Effect.Emitters = Setup;
			for (std::size_t Emitter = 0; Emitter < Setup.size(); ++Emitter)
			{
				const int32 Count = Setup[Emitter].Count;
				Effect.Particles[Emitter].assign(Count > 0 ? static_cast<std::size_t>(Count) : 0, FFieldParticle{});
			}
			return true;
		}
		return false;
	}

	void FFieldParticles::Stop(int16 Actor, bool bKill)
	{
		for (FFieldParticleEffect& Effect : Effects)
		{
			if (Effect.Actor != Actor || !Effect.bActive)
			{
				continue;
			}
			Effect.Emitters[0].EmitTicks = 0;
			Effect.Emitters[0].Delay = 0;
			for (std::size_t Emitter = 0; Emitter < Effect.Emitters.size(); ++Emitter)
			{
				if (Effect.Emitters[Emitter].Count == 0)
				{
					continue;
				}
				Effect.Emitters[Emitter].EmitTicks = 0;
				if (bKill)
				{
					for (FFieldParticle& Particle : Effect.Particles[Emitter])
					{
						Particle.Life = 1;
					}
				}
			}
		}
	}

	void FFieldParticles::End(int32 Index)
	{
		FFieldParticleEffect& Effect = Effects[static_cast<std::size_t>(Index)];
		Effect.bActive = false;
		Effect.Actor = FFieldParticleEffect::NoActor;
	}

	void FFieldParticles::ClearDrawn()
	{
		for (FFieldParticleEffect& Effect : Effects)
		{
			if (!Effect.bActive)
			{
				continue;
			}
			for (std::vector<FFieldParticle>& Emitted : Effect.Particles)
			{
				for (FFieldParticle& Particle : Emitted)
				{
					Particle.bDrawn = false;
				}
			}
		}
	}
}
