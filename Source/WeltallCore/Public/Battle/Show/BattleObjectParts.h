#pragma once

#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/Vram.h"

#include <array>
#include <vector>

namespace Weltall
{
	/** How a CLUT animation's level moves with its phase. */
	enum class EBattleClutCurve : uint8
	{
		/** base + (cos(phase) + 4096) / divisor. */
		Cosine,
		/** base + phase / divisor, ended once past 32. */
		Linear,
		/** base - phase / divisor. */
		Down,
		/** 32 - phase / divisor, no lower than base. */
		Up,
	};

	/**
	 * One of Battle's CLUT animations (0x30 bytes in the original): a block of colours faded toward black
	 * (type 0) or blended between two sources (1), or a block of the colour matrix scaled (4) or blended
	 * (5), its level 0-32 moving each frame along its curve. A source is read from VRAM (or the colour
	 * matrix) or filled with a colour; a result goes back to VRAM, or into the first source of the
	 * animation it feeds. A negative level ends it, the colours it read put back.
	 */
	struct FBattleClutAnimation
	{
		/** What a result feeds besides another animation of the same object: nothing (the result goes to VRAM), or the stage's tint. */
		static constexpr int16 FeedsNothing = -1;
		static constexpr int16 FeedsStageTint = -2;

		bool bActive = false;
		uint8 Type = 0;
		/** An animation feeding this one has written into its first source. */
		bool bFed = false;
		/** The animation of the same object this one's result feeds, or one of the two above. */
		int16 Feeds = FeedsNothing;
		/** How many colours: the rectangle's width (even for the VRAM types) times its height. */
		int16 Count = 0;
		int16 Phase = 0;
		int16 Speed = 0;
		/** The level last applied, -1 before the first. */
		int16 Level = -1;
		int16 Divisor = 0;
		int16 Base = 0;
		EBattleClutCurve Curve = EBattleClutCurve::Cosine;
		/** Types 4 and 5 work on the colour matrix; the others on VRAM. */
		bool bColourMatrix = false;
		FVramRect Rect;
		/** The first source (kept to be put back), the second, and the result; each there when its flag asked for it. */
		std::vector<uint16> Source;
		std::vector<uint16> Second;
		std::vector<uint16> Result;
		bool bHasSource = false;
		bool bHasSecond = false;
		bool bHasResult = false;
	};

	/**
	 * A trail emitter of an object (70h bytes): two points on a bone sampled each frame, on the screen
	 * (kind 0) or in the world (kind 1), which draw into a quad of the trails' pool, a new quad every few
	 * frames. A bone of -1 is an emitter switched off.
	 */
	struct FBattleTrailEmitter
	{
		static constexpr int32 SampleWords = 16;

		int16 Bone = -1;
		uint8 Kind = 0;
		/** Its quads are semi-transparent (added) when not 0, opaque otherwise. */
		uint8 Blend = 0;
		/** The quad of the pool it draws into (the pool's spare quad when the pool is full), -1 none. */
		int32 Segment = -1;
		std::array<FGteVector, 2> Points{};
		/**
		 * The last samples of the two ends, in sixteen words each kind reads its own way. Kind 0: the last
		 * eight screen points of each end, the latest at Ring (word i the first end's, x in its low half;
		 * word 8 + i the second's). Kind 1: the last two world points of each end, the latest at Ring's
		 * parity (words 4j to 4j + 2 the first end's, 8 + 4j to 10 + 4j the second's).
		 */
		std::array<int32, SampleWords> Samples{};
		int16 Ring = 0;
		/** Samples since the current quad started; -1 before the first (a started emitter's). */
		int16 Counter = 0;
		/** Samples a quad takes, at most seven. */
		int16 SegmentSamples = 0;
		/** Steps a quad lasts, its colour (64ths) and the fade taken off it each step. */
		int16 Life = 0;
		std::array<int16, 3> Colour{};
		std::array<int16, 3> Fade{};
	};

	/** A quad of the trails' pool (7Ch bytes): its four corners, its colour fading each step until its life runs out. */
	struct FBattleTrailSegment
	{
		/** Free when -1. */
		int16 Age = -1;
		/** 0: corners on the screen (x, y); 1: corners in the world. */
		int16 Kind = 0;
		/** The first end's start and end, the second end's start and end. */
		std::array<FGteVector, 4> Corners{};
		int16 Life = 0;
		std::array<int16, 3> Colour{};
		std::array<int16, 3> Fade{};
		/** Drawn semi-transparent (added, the pool's page's mode), as the pool makes its quads; a quad taken is as its emitter asks. */
		bool bSemiTransparent = true;
	};

	/** One of the two point lights key event 2 switches on (14h bytes). */
	struct FBattlePointLight
	{
		/** Where it is this frame. */
		FGteVector Position{};
		/** 0 off. */
		int16 Strength = 0;
		/** Its offset: in the world, or from its object's bone. */
		FGteVector Offset{};
		/** The slot whose bone carries it, -1 the world (0 until a key event places it); the bone counts from bone 1. */
		int16 Slot = 0;
		int16 Bone = 0;
	};

	/**
	 * The model camera (sequence ops 65-6A): nine tracks in the objects' pool, the words seven of them
	 * drive and the look-at and the eye the other two place, with the handshake a sequence waits on.
	 */
	struct FBattleModelCamera
	{
		static constexpr int32 TrackCount = 9;
		static constexpr int32 TargetTrack = 7;
		static constexpr int32 EyeTrack = 8;

		/** The model camera drives the camera. */
		bool bOn = false;
		/** The tag of the tracks a sequence waits on (FFh none; 0 as the battle starts, its memory cleared). */
		uint8 Watched = 0;
		/** The step's running (1) and ended (2) bits for the watched tag. */
		uint8 Events = 0;
		/** The look-at and the eye tracks follow their targets this step. */
		bool bFollow = false;
		/** The eye's yaw and pitch about the look-at, its distance and its height; the look-at's yaw, distance and height about its target. */
		int16 EyeYaw = 0;
		int16 EyePitch = 0;
		int16 EyeDistance = 0;
		int16 EyeHeight = 0;
		int16 TargetYaw = 0;
		int16 TargetDistance = 0;
		int16 TargetHeight = 0;
		/** Each track's channel in the objects' pool, -1 none. */
		std::array<int16, TrackCount> Tracks{-1, -1, -1, -1, -1, -1, -1, -1, -1};
		/** The value channel Track (0-6) moves: 0 the eye's yaw, 1 the look-at's, 2 the eye's pitch, 3 its distance, 4 the look-at's, 5 its height, 6 the eye's. */
		int16& Channel(int32 Track)
		{
			switch (Track)
			{
			case 0:
				return EyeYaw;
			case 1:
				return TargetYaw;
			case 2:
				return EyePitch;
			case 3:
				return EyeDistance;
			case 4:
				return TargetDistance;
			case 5:
				return TargetHeight;
			default:
				return EyeHeight;
			}
		}
		/** The floor clamp: the triangle it last found, the height it gave and the frame it looked. */
		int16 FloorTriangle = -1;
		int16 FloorHeight = 32000;
		int16 FloorFrame = -1;
	};
}
