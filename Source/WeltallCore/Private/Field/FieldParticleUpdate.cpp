#include "Field/FieldParticles.h"

#include "Field/EventContext.h"
#include "Game/PsxRandom.h"
#include "Graphics/Gte.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"
#include "MovementRules.h"

#include "Foundation/ProcessorDivide.h"

#include <string>

namespace Weltall
{
	namespace
	{
		using MovementRules::Multiply;
		using MovementRules::WholePart;
		using MovementRules::Wrap;

		constexpr int16 Endless = 0x7FFF;
		constexpr int32 AngleMask = 0xFFF;
		constexpr int16 FullScale = 0x1000;
		/** The facing offsets' slot for each eighth of a turn the owner faces on screen. */
		constexpr std::array<uint8, 8> FacingSlots = {2, 3, 4, 5, 6, 7, 0, 1};

		FError ParticleError(int32 Effect, const std::string& What)
		{
			return MakeError("particle effect " + std::to_string(Effect) + ": " + What);
		}

		/** rand() scaled into 0..Limit, as the field draws a particle's spread. */
		int32 RandomBelow(FEventContext& Context, uint16 Limit)
		{
			return Wrap(int64{Context.Random->Next()} * Limit + 1) >> 15;
		}

		/** A colour channel stepped and held within 0 to 255. */
		uint8 StepChannel(uint8 Channel, int8 Step)
		{
			const int32 Value = Channel + Step;
			return static_cast<uint8>(Step < 0 ? std::max(Value, 0) : std::min(Value, 0xFF));
		}

		/**
		 * A dead particle born again: its delay staggered after the tick's earlier births, a start on
		 * or within the spawn circle about the origin (moved for the owner's facing on screen), and a
		 * heading toward the target spread along the start's direction.
		 */
		void Respawn(FEventContext& Context, const FFieldParticleEmitter& Emitter, const FEventActorState& Owner, FFieldParticle& Particle, int32& Stagger)
		{
			Particle.bAlive = true;
			Particle.bJointless = false;
			Particle.Delay = static_cast<int16>(Emitter.Interval + Stagger);
			Stagger += static_cast<uint16>(Emitter.Interval);
			Particle.Life = Emitter.Lifetime;
			Particle.Roll = (Emitter.Flags & FieldParticleFlags::RandomRoll) != 0 ? static_cast<int16>(Context.Random->Next() & AngleMask) : Emitter.Roll;
			const uint32 Radius = (Emitter.Flags & FieldParticleFlags::OnCircle) != 0 ? uint32{static_cast<uint16>(Emitter.SpawnRadius)}
																						 : static_cast<uint32>(RandomBelow(Context, static_cast<uint16>(Emitter.SpawnRadius)));
			const int32 Angle = RandomBelow(Context, AngleMask);
			const int32 AcrossZ = (Emitter.Flags & FieldParticleFlags::AlongX) != 0 ? 0 : Multiply(GteSin(Angle), Radius) >> 12;
			const int32 Sector = ((Context.Camera.ViewYaw + static_cast<int16>(Owner.Yaw)) & AngleMask) >> 9;
			const std::array<int16, 2>& Offset = Emitter.FacingOffsets[FacingSlots[static_cast<std::size_t>(Sector)]];
			const int32 X = (Multiply(GteCos(Angle), Radius) >> 12) + Emitter.Origin[0] + Offset[0];
			const int32 Z = AcrossZ + Emitter.Origin[2] + Offset[1];
			Particle.Position = {X, Emitter.Origin[1], Z};
			const int32 Spread = RandomBelow(Context, static_cast<uint16>(Emitter.TargetSpread));
			Particle.Velocity = {Emitter.Target[0] + (Multiply(GteCos(Angle), Spread) >> 12) - X, Emitter.Target[1] - Emitter.Origin[1],
				Emitter.Target[2] + (Multiply(GteSin(Angle), Spread) >> 12) - Z};
			Particle.Acceleration = {Emitter.Acceleration[0], Emitter.Acceleration[1], Emitter.Acceleration[2]};
			Particle.Size = Emitter.Size;
			Particle.SizeStep = Emitter.SizeStep;
			Particle.Color = Emitter.Color;
			Particle.ColorStep = Emitter.ColorStep;
		}

		FGteVector Short(const std::array<int32, 3>& Vector)
		{
			return {static_cast<int16>(Vector[0]), static_cast<int16>(Vector[1]), static_cast<int16>(Vector[2])};
		}

		/**
		 * A particle placed in the world as its delay ends: its heading turned into the emitter's space
		 * and set to the speed, its start turned and added to where the space puts the owner (a scaled
		 * owner's also tilted to the view and scaled), in 20.12. The emitter's scale becomes the owner's
		 * for a scaled owner, 1 otherwise.
		 */
		std::optional<FError> Place(FEventContext& Context, FFieldParticleEmitter& Emitter, FFieldParticle& Particle, int32 Effect)
		{
			const EFieldParticleSpace Space = Emitter.Space();
			FGteMatrix Rotation;
			FGteLongVector Base{};
			bool bScaled = false;
			if (Space == EFieldParticleSpace::GearJoint)
			{
				if (!Context.Gears.Models || Emitter.GearSlot < 0 || Emitter.GearSlot >= FGearModels::Slots)
				{
					return ParticleError(Effect, "an emitter on the joint of a gear slot the module does not have, or on a map whose load loaded no gears");
				}
				const std::optional<FGteMatrix> Joint = FieldGearJoint(Context, Emitter.GearSlot, Emitter.GearJoint);
				if (Context.Gears.Models->Fault)
				{
					return Context.Gears.Models->Fault;
				}
				// A slot without a model, or a joint its model does not have: the original's call writes no matrix
				// and the particle is turned by whatever its stack held, about the map's origin. It is placed there
				// unturned, and kept out of the picture.
				Particle.bJointless = !Joint;
				Rotation = Joint ? *Joint : FGteMatrix::Identity();
				Base = RotTrans(Rotation, Emitter.Origin);
				Emitter.Scale = FullScale;
			}
			else if (Space == EFieldParticleSpace::OwnerModel)
			{
				if (Emitter.Owner < 0 || static_cast<std::size_t>(Emitter.Owner) >= Context.Actors.size())
				{
					return ParticleError(Effect, "its owner " + std::to_string(Emitter.Owner) + " is not an actor");
				}
				const FFieldActor& Record = Context.Actors[static_cast<std::size_t>(Emitter.Owner)];
				Rotation.M = Record.WorldMatrix;
				Rotation.T = Record.Position;
				Base = RotTrans(Rotation, Emitter.Origin);
				Emitter.Scale = FullScale;
			}
			else
			{
				const FEventActorState* Owner = Context.StateOf(Emitter.Owner);
				if (Owner == nullptr)
				{
					return ParticleError(Effect, "its owner " + std::to_string(Emitter.Owner) + " has no script state");
				}
				Rotation = RotationXyz({0, static_cast<int16>(Owner->Yaw), 0});
				Base = {WholePart(Owner->Position[0]), WholePart(Owner->Position[1]), WholePart(Owner->Position[2])};
				bScaled = Space == EFieldParticleSpace::ScaledOwner;
				Emitter.Scale = bScaled ? Owner->Scale[0] : FullScale;
			}
			Rotation.T = {};
			const FGteLongVector Heading = ApplyMatrix(Rotation, Short(Particle.Velocity));
			// libgte adds the squares with a trapping add: a sum past 31 bits would stop the console.
			const int64 Squares = int64{GteSquare(Heading[0])} + GteSquare(Heading[1]) + GteSquare(Heading[2]);
			if (Squares > 0x7FFFFFFF)
			{
				return ParticleError(Effect, "a heading's squares pass 31 bits, which stops the original");
			}
			const FGteLongVector Normal = VectorNormal(Heading);
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Particle.Velocity[Axis] = Multiply(Multiply(Normal[Axis], Emitter.Speed) >> 12, Emitter.SpeedScale);
				if (bScaled)
				{
					Particle.Position[Axis] = Multiply(Particle.Position[Axis], Emitter.Scale) >> 12;
				}
			}
			const FGteLongVector Turned = RotTrans(Rotation, Short(Particle.Position));
			if (!bScaled)
			{
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Particle.Position[Axis] = Multiply(int64{Base[Axis]} + Turned[Axis], FullScale);
				}
				return std::nullopt;
			}
			// The height is tilted with the view, so the owner's scaled effect keeps its size on screen.
			const FGteMatrix View = RotationZyx({static_cast<int16>(static_cast<int16>(Context.Camera.ViewPitch) - 0x400), static_cast<int16>(-Context.Camera.ViewYaw), 0});
			const std::array<int32, 3> Tilted = ApplyMatrixLV(View, {0, Turned[1], 0});
			const int32 Unscale = ProcessorDivide(0x1000000, Emitter.Scale);
			Particle.Position = {Multiply(int64{Base[0]} + Turned[0] + Tilted[0], Unscale), Multiply(int64{Base[1]} + Tilted[1], Unscale),
				Multiply(int64{Base[2]} + Turned[2] + Tilted[2], Unscale)};
			return std::nullopt;
		}

		/** A particle's tick: placed when its delay ends, or once placed moved, grown and shaded, its life counted down. */
		std::optional<FError> Step(FEventContext& Context, FFieldParticleEmitter& Emitter, FFieldParticle& Particle, int32 Effect)
		{
			if (Particle.Delay != 0)
			{
				const int16 Delay = Particle.Delay;
				Particle.Delay = static_cast<int16>(Delay - 1);
				return Delay == 1 ? Place(Context, Emitter, Particle, Effect) : std::nullopt;
			}
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Particle.Velocity[Axis] = Wrap(int64{Particle.Velocity[Axis]} + Particle.Acceleration[Axis]);
				Particle.Size[Axis] = static_cast<int16>(Particle.Size[Axis] + Particle.SizeStep[Axis]);
				Particle.Position[Axis] = Wrap(int64{Particle.Position[Axis]} + Particle.Velocity[Axis]);
				Particle.Color[Axis] = StepChannel(Particle.Color[Axis], Particle.ColorStep[Axis]);
			}
			// The field draws the particle as it moves it, but for its last tick.
			Particle.bDrawn = Particle.Life != 1 && !Particle.bJointless;
			Particle.Life = static_cast<int16>(Particle.Life - 1);
			if (Particle.Life == 0)
			{
				Particle.bAlive = false;
			}
			return std::nullopt;
		}
	}

	std::optional<FError> UpdateFieldParticles(FEventContext& Context)
	{
		// With their textures kept aside (a map's entry, a large image), the particles stand still.
		if (Context.Render.bEffectTexturesSaved)
		{
			return std::nullopt;
		}
		FFieldParticles& Particles = Context.Particles;
		for (int32 Index = 0; Index < FFieldParticles::EffectCount; ++Index)
		{
			FFieldParticleEffect& Effect = Particles.Effects[static_cast<std::size_t>(Index)];
			if (!Effect.bActive)
			{
				continue;
			}
			bool bBusy = false;
			for (std::size_t Slot = 0; Slot < Effect.Emitters.size(); ++Slot)
			{
				FFieldParticleEmitter& Emitter = Effect.Emitters[Slot];
				if (Emitter.Count == 0)
				{
					continue;
				}
				if (Emitter.Delay != 0)
				{
					bBusy = true;
					--Emitter.Delay;
					continue;
				}
				int32 Stagger = 0;
				for (FFieldParticle& Particle : Effect.Particles[Slot])
				{
					if (!Particle.bAlive)
					{
						if (Emitter.EmitTicks == 0)
						{
							continue;
						}
						const FEventActorState* Owner = Context.StateOf(Emitter.Owner);
						if (Owner == nullptr)
						{
							return ParticleError(Index, "its owner " + std::to_string(Emitter.Owner) + " has no script state");
						}
						Respawn(Context, Emitter, *Owner, Particle, Stagger);
					}
					if (auto Error = Step(Context, Emitter, Particle, Index))
					{
						return Error;
					}
					bBusy = true;
				}
				if (Emitter.EmitTicks != 0)
				{
					if (Emitter.EmitTicks != Endless)
					{
						--Emitter.EmitTicks;
					}
					bBusy = true;
				}
			}
			if (!bBusy)
			{
				Particles.End(Index);
			}
		}
		return std::nullopt;
	}
}
