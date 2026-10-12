#pragma once

#include "Battling/BattlingSkeleton.h"
#include "Foundation/Types.h"

#include <array>

namespace Weltall
{
	/** What an effect slot shows. */
	enum class EBattlingEffectKind : uint8
	{
		/** An animated sprite of one of the five sets (hit flashes, dust, fizzles), rising for some. */
		Sprite,
		/** A trail behind one point: an attack's joint, a projectile. */
		Trail,
		/** A band swept between two points: an attack's segment, the dashing body. */
		Swept,
	};

	/** One of the 60 effect slots. */
	struct FBattlingEffect
	{
		bool bActive = false;
		uint8 Frame = 0;
		/** The frame it ends on. */
		uint8 Count = 0;
		EBattlingEffectKind Kind = EBattlingEffectKind::Sprite;
		/** A sprite's set (0-4). */
		uint8 Set = 0;
		std::array<int16, 3> Position{};
		/** A sprite's rise: its acceleration and its speed. */
		int16 RiseAcceleration = 0;
		int16 RiseSpeed = 0;
		/** Set 2 takes the effect colour of its spawn; the others are drawn untinted. */
		std::array<uint8, 3> Colour{};
		bool bTinted = false;
		/** A trail or band: the pass it was added on (low 16 bits), the effects' tick it was added on, and its source's id. */
		uint16 Pass = 0;
		uint16 Tick = 0;
		uint32 Id = 0;
		/** A trail's or a band's side, in whose gear's colour it is drawn. */
		uint8 Side = 0;
		/** A trail's segment count. */
		int16 Length = 0;
		/** The same source's segment of the tick before, -1 none. */
		int8 Link = -1;
		/** A band's second point. */
		std::array<int16, 3> End{};
	};

	/** A coloured line drawn for one tick: projectiles, the rubber band's chain. */
	struct FBattlingLine
	{
		std::array<int16, 3> From{};
		std::array<int16, 3> To{};
		std::array<uint8, 3> Colour{};
	};

	/** A grain of sand kicked up, rising and falling back to the ground it started on. */
	struct FBattlingDust
	{
		std::array<int16, 3> Position{};
		bool bLive = false;
		int8 Speed = 0;
		int16 Ground = 0;
	};

	/** A drop of water thrown up by a gear moving in the lake. */
	struct FBattlingSplash
	{
		std::array<int16, 3> Position{};
		int16 Life = 0;
		int8 SpeedX = 0;
		int8 SpeedZ = 0;
		int16 SpeedY = 0;
		/** The size of the frame's tile of the same number (the frame's nth splash is drawn as its nth tile), drawn as the program starts. */
		int16 Width = 0;
		int16 Height = 0;
	};

	/** A spark of the hit emitter. */
	struct FBattlingSpark
	{
		std::array<int16, 3> Position{};
		int16 Life = 0;
		std::array<int16, 3> Velocity{};
		/** Its tile's width and height. */
		int16 Width = 0;
		int16 Height = 0;
	};

	/** Everything the fight shows besides the gears, as the program keeps it. */
	struct FBattlingEffects
	{
		static constexpr int32 SlotCount = 60;
		static constexpr int32 LineCount = 100;
		static constexpr int32 DustCount = 255;
		/** Spawning looks at the first 251 grains, stepping at the first 252. */
		static constexpr int32 DustSpawnCount = 251;
		static constexpr int32 DustStepCount = 252;
		static constexpr int32 SplashCount = 540;
		/** Spawning and stepping look at the first 536. */
		static constexpr int32 SplashUsedCount = 536;
		static constexpr int32 SparkCount = 0x60;

		std::array<FBattlingEffect, SlotCount> Slots{};
		/** The colour the next tinted effects take: the gear's. */
		std::array<uint8, 3> Colour{};
		/** Counts the effects' ticks, linking a source's trail segments one tick to the next. */
		uint16 Tick = 0x8000;
		std::array<FBattlingLine, LineCount> Lines{};
		/** Lines added this tick, and the ones the frame draws (the last tick's). */
		int32 LinesAdded = 0;
		int32 LinesShown = 0;
		std::array<FBattlingDust, DustCount> Dust{};
		std::array<FBattlingSplash, SplashCount> Splashes{};
		std::array<FBattlingSpark, SparkCount> Sparks{};
		/** Where this tick's hit sparks start (the sparks keep 16 bits of it), and how many it asks for. */
		FBattlingPoint SparkSource{};
		int32 SparkRequest = 0;

		/**
		 * What the frame draws of the particles, as the steps the drawing takes leave them: each grain of dust stepped, where
		 * it was before its step; each splash, moved across but not yet up or down; and whether each spark was alive as its
		 * step began (it is drawn where the step took it).
		 */
		std::array<std::array<int16, 3>, DustStepCount / 2> DrawnDust{};
		int32 DrawnDustCount = 0;
		std::array<std::array<int16, 3>, SplashUsedCount> DrawnSplashes{};
		int32 DrawnSplashCount = 0;
		std::array<bool, SparkCount> SparksDrawn{};
	};
}
