#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"

#include <array>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	class FPsxRandom;
	struct FGteArctangents;

	/**
	 * A particle emitter of the scene's effects (section 5: 64 effects of 8 emitters, 0x54 bytes
	 * each), as the program runs it in place: its setup from the file and what it keeps while it
	 * emits.
	 */
	struct FWorldMapEmitter
	{
		/** The delay's two counters as the effect starts them: ticks before the first spawn, then spawns. */
		uint32 StartDelay = 0;
		uint32 Delay = 0;
		int16 MaxParticles = 0;
		int16 Alive = 0;
		int16 Life = 0;
		/** The shape its particles are drawn as (1-9), which each keeps as its mark of life; 0 for none. */
		int16 Shape = 0;
		int16 Interval = 0;
		int16 Countdown = 0;
		/** Where the effect was started (whole units), and the word the program copies after it. */
		std::array<int16, 3> Place{};
		int16 PlaceTail = 0;
		/** The effect's rotation (angles), and a pad the file keeps. */
		FGteVector Angles{};
		int16 AnglesPad = 0;
		/** The spawn point's offset and the spread's axis, turned by the angles. */
		FGteVector Offset{};
		int16 OffsetPad = 0;
		FGteVector Axis{};
		int16 AxisPad = 0;
		int32 Speed = 0;
		std::array<int16, 3> Acceleration{};
		int16 AccelerationPad = 0;
		/** The spawn's distance along the random direction, and the spread's. */
		uint16 Distance = 0;
		uint16 Spread = 0;
		/** Handed to each particle: its size and growth (two shorts each), its colour and colour step. */
		uint32 Size = 0;
		uint32 Growth = 0;
		/** Red, green, blue and the flags byte: bit 7 emitting, 4 a delayed start, 5 and 6 no vertical spread, 2 and 3 a fixed distance, 0-1 the blend. */
		uint32 Colour = 0;
		uint32 ColourStep = 0;

		uint8 Flags() const { return static_cast<uint8>(Colour >> 24); }
		void SetFlags(uint8 Flags) { Colour = (Colour & 0x00FFFFFF) | static_cast<uint32>(Flags) << 24; }
	};

	/** A living particle: its emitter, turn, life, place, speed, acceleration, size and colour (0x4C bytes in the original). */
	struct FWorldMapParticle
	{
		int16 Emitter = 0;
		int16 Angle = 0;
		int16 Life = 0;
		/** The shape it is drawn as, its emitter's, while it lives; 0 once it has died. */
		int16 Shape = 0;
		std::array<int32, 3> Position{};
		std::array<int32, 3> Velocity{};
		std::array<int32, 3> Acceleration{};
		std::array<int16, 2> Size{};
		std::array<int16, 2> Growth{};
		uint32 Colour = 0;
		uint32 ColourStep = 0;
		/** The texture page it is drawn from, with the blend its emitter gives it. */
		uint16 TexturePage = 0;
	};

	/** The world map's particles: the scene's emitters and 256 particles. */
	class FWorldMapParticles
	{
	public:
		static constexpr int32 EmittersPerEffect = 8;
		static constexpr int32 EmitterBytes = 0x54;
		static constexpr int32 ParticleCount = 256;
		/** The shapes the program draws particles as: 0 none, then nine (their corners and texels are its tables). */
		static constexpr int32 ShapeCount = 10;

		/** The scene's effects as the particles' start leaves them. */
		WELTALLCORE_API static TResult<FWorldMapParticles> FromSection(std::span<const uint8> Section);

		/**
		 * The effect's emitters set going (unless its first already runs) at a
		 * place (and the word after it) and turned by the negated rotation, each missing one zero.
		 */
		WELTALLCORE_API void Start(int32 Effect, const std::optional<std::array<int16, 4>>& Place, const std::optional<FGteVector>& Rotation);
		/** The effect's emitters stop spawning. */
		WELTALLCORE_API void Stop(int32 Effect);
		/** Every effect's emitters stop spawning (the port's: the original has no such call). */
		WELTALLCORE_API void StopAll();
		/** The effect's living particles' lives ended, so that the next tick frees them undrawn. */
		WELTALLCORE_API void StopParticles(int32 Effect);
		/** Every emitter's tick (spawning with rand()), then every particle's. */
		WELTALLCORE_API void Tick(FPsxRandom& Random, const FGteArctangents& Arctangents);

		const std::vector<FWorldMapEmitter>& Emitters() const { return EmitterList; }
		const std::array<FWorldMapParticle, ParticleCount>& Particles() const { return ParticleList; }

	private:
		void Spawn(int32 Index, uint8 Flags, FPsxRandom& Random, const FGteArctangents& Arctangents);
		void Move();

		std::vector<FWorldMapEmitter> EmitterList;
		std::array<FWorldMapParticle, ParticleCount> ParticleList{};
	};
}
