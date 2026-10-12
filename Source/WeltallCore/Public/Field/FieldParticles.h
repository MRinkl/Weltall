#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <array>
#include <optional>
#include <vector>

namespace Weltall
{
	struct FEventContext;

	/** Bits of FFieldParticleEmitter::Flags. */
	namespace FieldParticleFlags
	{
		constexpr uint16 RandomRoll = 0x1;
		/** Bits 1-2: where the quads sort: in front of everything, or at their depth less 16, at it, or 16 more. */
		constexpr uint16 DepthShift = 1;
		/** Bits 4-5: what the emitter's points are relative to (EFieldParticleSpace). */
		constexpr uint16 SpaceShift = 4;
		/** No spread across z: particles start along the x axis only. */
		constexpr uint16 AlongX = 0x40;
		/** Particles start on the spawn circle, not within it. */
		constexpr uint16 OnCircle = 0x80;
		/** Bits 8-9: the quads' blend, the GPU's mode (bits + 1) & 3. */
		constexpr uint16 BlendShift = 8;
	}

	/** Where an emitter's origin and target are: around its owner, on a gear's joint, in its owner's model, or scaled with its owner. */
	enum class EFieldParticleSpace : uint8
	{
		Owner,
		GearJoint,
		OwnerModel,
		ScaledOwner,
	};

	/**
	 * One of an effect's particles as its emitter moves it: after a delay it is placed in the world,
	 * then each tick its velocity, size, position and colour change until its life runs out.
	 */
	struct FFieldParticle
	{
		bool bAlive = false;
		int16 Delay = 0;
		int16 Life = 0;
		/** Its quad's turn about the view's axis, 4096 a turn. */
		int16 Roll = 0;
		/** Until it is placed, its start about the emitter's origin; then 20.12 world units. */
		std::array<int32, 3> Position{};
		/** Until it is placed, toward its target; then 20.12 per tick. */
		std::array<int32, 3> Velocity{};
		std::array<int32, 3> Acceleration{};
		/** Its quad's scale on each axis, 4096 is 1, and the change per tick. */
		std::array<int16, 3> Size{};
		std::array<int16, 3> SizeStep{};
		std::array<uint8, 3> Color{};
		std::array<int8, 3> ColorStep{};
		/** The tick's update moved it and drew it: not in its last tick of life, nor waiting or just placed. */
		bool bDrawn = false;
		/**
		 * Placed on a gear's joint while the gear's slot held no model, as in the ticks between a gear freed and
		 * the next one loaded: it lives its ticks about the map's origin and is never drawn.
		 */
		bool bJointless = false;
	};

	/**
	 * An emitter as the scripts set it up (FE 8F-95, A5, BD, C8, C9) and an effect keeps its copy: how
	 * many particles it keeps alive and for how long it respawns them, where they start and head,
	 * and how they look. The fields are the original's, in its order.
	 */
	struct FFieldParticleEmitter
	{
		int16 Word00 = 0;
		/** Ticks before it starts. */
		int16 Delay = 0;
		/** Ticks it respawns dead particles for, 0x7FFF for ever. */
		int16 EmitTicks = 0;
		/** Its particles; 0 leaves it out. */
		int16 Count = 0;
		/** The particles' speed, 20.12. */
		int32 Speed = 0;
		std::array<int16, 3> Origin{};
		std::array<int16, 3> Target{};
		std::array<int16, 3> Acceleration{};
		int16 SpeedScale = 0;
		int16 SpawnRadius = 0;
		/** How far from the target each particle's heading may land. */
		int16 TargetSpread = 0;
		/** FieldParticleFlags bits. */
		uint16 Flags = 0;
		/** An offset of the start for each eighth of a turn the owner faces on screen. */
		std::array<std::array<int16, 2>, 8> FacingOffsets{};
		/** 4096 is 1; a scaled owner's effect takes its owner's. */
		int16 Scale = 0;
		/** The actor the emitter is placed on. */
		int16 Owner = 0;
		/** The particles' picture in the field's particle texture. */
		int16 Sprite = 0;
		/** The ticks between particles respawned in one tick. */
		int16 Interval = 0;
		int16 Lifetime = 0;
		std::array<int16, 3> Size{};
		std::array<int16, 3> SizeStep{};
		std::array<uint8, 3> Color{};
		std::array<int8, 3> ColorStep{};
		int16 GearSlot = 0;
		int16 GearJoint = 0;
		int16 Roll = 0;

		EFieldParticleSpace Space() const { return static_cast<EFieldParticleSpace>(Flags >> FieldParticleFlags::SpaceShift & 3); }
	};

	/** A running effect: its emitters, copied from the setup when it started, and their particles. */
	struct FFieldParticleEffect
	{
		static constexpr int16 NoActor = -1;

		bool bActive = false;
		/** The actor whose script started it, which stops it. */
		int16 Actor = NoActor;
		std::array<FFieldParticleEmitter, 8> Emitters{};
		std::array<std::vector<FFieldParticle>, 8> Particles{};
	};

	/**
	 * The field's particle effects: the emitters the scripts set up and the 64 effects started from
	 * them. Each tick the effects respawn and move their particles and end once nothing is left.
	 */
	struct FFieldParticles
	{
		static constexpr int32 EmitterCount = 8;
		static constexpr int32 EffectCount = 64;

		std::array<FFieldParticleEmitter, EmitterCount> Setup{};
		/** FE 8F's owner, space (as flag bits), gear slot and joint, and FE 90's emitter. */
		int32 Owner = 0;
		int32 SpaceBits = 0;
		int32 GearSlot = 0;
		int32 GearJoint = 0;
		int32 Current = 0;
		std::array<FFieldParticleEffect, EffectCount> Effects{};

		/** Every emitter of the setup at its defaults, placed on Actor. */
		WELTALLCORE_API void ResetSetup(int16 Actor);
		/** Starts an effect from the setup in the first free slot, for Actor's script to stop; false when all are running. */
		WELTALLCORE_API bool Start(int16 Actor);
		/** Actor's effects stop respawning; with bKill their particles end on the next tick too. */
		WELTALLCORE_API void Stop(int16 Actor, bool bKill);
		WELTALLCORE_API void End(int32 Effect);
		/** A tick begins: no particle is drawn until its update draws it. */
		WELTALLCORE_API void ClearDrawn();
	};

	/**
	 * One tick of the particle effects, as the field runs them after its characters: each emitter
	 * past its delay respawns its dead particles while it emits (with the game's rand()), and every
	 * living particle is placed or moved; an effect with nothing left ends. An error when a particle
	 * is placed on an actor without a script state or on a gear's joint (the gears' module).
	 */
	WELTALLCORE_API std::optional<FError> UpdateFieldParticles(FEventContext& Context);
}
