#pragma once

#include "Foundation/Types.h"

#include <array>

namespace Weltall
{
	/** Bits of an actor's first flag word, which scripts read and write whole. */
	namespace EventActorFlags0
	{
		/** Its scripts do not run, and it neither moves nor collides. */
		constexpr uint32 Frozen = 0x1;
		constexpr uint32 PassesThroughActors = 0x80;
		/** What its model is: a sprite of the map's, a pilot in a gear, a party member. */
		constexpr uint32 MapSprite = 0x100;
		constexpr uint32 GearPilot = 0x200;
		constexpr uint32 PartyMember = 0x400;
		/** The same bits are its collision classes: floors of a class it has are closed to it. */
		constexpr uint32 CollisionClasses = 0x700;
		/** In a jump, from its start until it lands. */
		constexpr uint32 Airborne = 0x800;
		/** Above its floor and falling onto it. */
		constexpr uint32 Falling = 0x1000;
		/** Jumping or falling. */
		constexpr uint32 InTheAir = Airborne | Falling;
		constexpr uint32 PadControlled = 0x4000;
		/** Its facing stays the locked facing while it moves. */
		constexpr uint32 FacingLocked = 0x8000;
		/** A script moves it: it neither walks nor takes its floor by itself. */
		constexpr uint32 MovedByScript = 0x10000;
		/** Held at its fixed floor height (state's TargetHeight) instead of the walkmesh's. */
		constexpr uint32 FixedHeight = 0x40000;
		/** Talking to it and touching it start nothing. */
		constexpr uint32 NoTalk = 0x20000;
		/** Set by the leader's control step on the other party members, cleared at every actor's turn. */
		constexpr uint32 PartyFollower = 0x1000000;
		/** Placed by a script: its next move settles it on its floor. */
		constexpr uint32 FollowsFloor = 0x400000;
		/** Its animation is left as it is. */
		constexpr uint32 AnimationLocked = 0x2000000;
		/** Its last move followed a step or a slope along the floor's rise. */
		constexpr uint32 MovedAlongSlope = 0x4000000;
		/** The actor is a door its scripts have opened (swung or slid). */
		constexpr uint32 DoorOpen = 0x100000;
		/** Set while a walk heads for the leader. */
		constexpr uint32 WalksToLeader = 0x200000;
		/** What a placed actor starts with: it cannot be pushed, passes through actors, and bit 5. */
		constexpr uint32 Initial = 0xB0;
	}

	/** Bits of an actor's second flag word. */
	namespace EventActorFlags1
	{
		/** Bit L: it stays out of walkmesh layer L. */
		constexpr uint32 LayersIgnored = 0x7;
		/** Bit L + 3: it ignores the attributes of layer L's floors. */
		constexpr uint32 AttributesIgnoredShift = 3;
		/** Its sprite's origin was off the original's screen when last drawn: unless it keeps running off screen, it neither moves nor animates. */
		constexpr uint32 OffScreen = 0x200;
		/** It moves and animates off screen too: the party's actors. */
		constexpr uint32 KeepsRunningOffScreen = 0x400;
		/** Its model is not aligned to the floor. */
		constexpr uint32 NotFloorAligned = 0x800;
		/** Its animation stands still: a ladder's, while it stands on one. */
		constexpr uint32 AnimationPaused = 0x1000;
		/** It is a gear: drawn as a gear's model. */
		constexpr uint32 Gear = 0x2000;
		/** Set when its sprite's animation ends or holds. */
		constexpr uint32 AnimationFinished = 0x10000;
		/** A gear moved by its model: the gears' module gives it its motion. */
		constexpr uint32 GearMovesIt = 0x20000;
		/** Its movement sets its sprite's velocity from its direction and speed. */
		constexpr uint32 MovesBySpriteVelocity = 0x80000;
		/** Taken out of the map: its scripts never run again and other scripts cannot start any. */
		constexpr uint32 Removed = 0x100000;
		/** Placed where a script put it: it takes no floor by itself. */
		constexpr uint32 ExplicitPosition = 0x200000;
		/** Its sprite has no animations: it is neither animated nor turned to face. */
		constexpr uint32 NoAnimations = 0x1000000;
		/** Its sprite is not drawn, though it keeps being updated. */
		constexpr uint32 SpriteHidden = 0x2000000;
		/** Its talk script ends at once, so talking to it starts nothing. */
		constexpr uint32 NoTalkScript = 0x4000000;
		constexpr uint32 Initial = NotFloorAligned;
	}

	/**
	 * One of an actor's eight script threads: where it is, what it waits for and how urgent it is.
	 * The last word packs several fields that some instructions read and write whole.
	 */
	struct FEventThread
	{
		static constexpr uint32 MoveCounterMask = 0xFFFF;
		static constexpr uint32 RequestWaitShift = 16;
		static constexpr uint32 RequestWaitMask = 0x3u << RequestWaitShift;
		static constexpr uint32 PriorityShift = 18;
		static constexpr uint32 PriorityMask = 0xFu << PriorityShift;
		/** Kept by an instruction that started a script in this slot and waits for it. */
		static constexpr uint32 Reserved = 1u << 22;
		static constexpr uint32 MoveStageMask = 0x3u << 23;

		/** 0 is the most urgent; a slot with priority Free holds no thread. */
		static constexpr uint32 Free = 0xF;
		static constexpr uint8 NoScript = 0xFF;

		uint16 Pc = 0;
		/** Ticks left of a timed wait. */
		uint8 WaitCount = 0;
		/** The script the thread runs, or NoScript. */
		uint8 Script = 0;
		/**
		 * Bits 0-15: a movement's remaining ticks (FFFF none); 16-17: the state of a wait on a script
		 * this thread started; 18-21: the priority; 22: Reserved; 23-24: a movement's stage.
		 */
		uint32 Word = 0;

		uint32 Priority() const { return Word >> PriorityShift & 0xF; }
		void SetPriority(uint32 Value) { Word = (Word & ~PriorityMask) | (Value & 0xF) << PriorityShift; }
		bool IsFree() const { return Priority() == Free; }
		bool IsReserved() const { return (Word & Reserved) != 0; }
		/** Free and not reserved: where a new thread may start. */
		bool IsAvailable() const { return IsFree() && !IsReserved(); }
		uint32 RequestWait() const { return Word >> RequestWaitShift & 0x3; }
		void SetRequestWait(uint32 Value) { Word = (Word & ~RequestWaitMask) | (Value & 0x3) << RequestWaitShift; }
	};

	/**
	 * A script actor's state: its flags, body, position and motion, its script threads and call
	 * stack, and what its scripts set for its model, messages and sound. The actor record (FFieldActor)
	 * holds the rest. A value-initialized state is the zeroed memory the field allocates; Reset gives
	 * it its defaults.
	 */
	struct FEventActorState
	{
		static constexpr int32 ThreadCount = 8;
		static constexpr int32 CallStackSize = 4;
		static constexpr int32 PartValueCount = 32;
		static constexpr uint32 CallDepthShift = 6;
		static constexpr uint32 CallDepthMask = 0x7u << CallDepthShift;

		uint32 Flags0 = 0;
		uint32 Flags1 = 0;
		/** The floor triangle under the actor on each walkmesh layer. */
		std::array<int16, 4> FloorTriangles{};
		uint16 Layer = 0;
		uint16 Word12 = 0;
		/** The attribute word of the floor triangle it stands on. */
		uint32 FloorAttributes = 0;
		uint16 HalfWidth = 0;
		uint16 Height = 0;
		uint16 HalfDepth = 0;
		uint16 Radius = 0;
		/** 16.16. */
		std::array<int32, 3> Position{};
		int32 Word2C = 0;
		std::array<int32, 3> Step{};
		int32 Word3C = 0;
		std::array<int32, 3> Velocity{};
		int32 Word4C = 0;
		/** The floor's normal where the actor was placed, 4096 long. */
		std::array<int32, 3> FloorNormal{};
		int32 Word5C = 0;
		/** Where others interact with it, relative to its position. */
		int16 InteractX = 0;
		int16 Word62 = 0;
		int16 InteractZ = 0;
		int16 Word66 = 0;
		std::array<int16, 3> PreviousPosition{};
		int16 StillTicks = 0;
		int16 ExtraAngle = 0;
		int16 FloorHeight = 0;
		/**
		 * An actor index, FF none: the talk and touch checks skip that actor when this is the leader,
		 * and a return to the field raises the leader by 8 while it is set.
		 */
		uint8 LinkedActor = 0;
		/** The actor whose model it is attached to, FF none. */
		uint8 Parent = 0;
		/** Walking moves 0x4000000 / SpeedDivisor per tick (16.16). */
		uint16 SpeedDivisor = 0;
		/** Return points of calls; shared by all the actor's threads. */
		std::array<uint16, CallStackSize> CallStack{};
		/** Portrait shown beside its messages, FF none. */
		uint8 Portrait = 0;
		/** The player's last choice in its message window. */
		uint8 Choice = 0;
		uint8 WindowWidth = 0;
		uint8 WindowLines = 0;
		/** Low half: the default window style; high half: the last message's own style. */
		uint32 WindowStyle = 0;
		uint16 WindowX = 0;
		uint16 WindowY = 0;
		std::array<FEventThread, ThreadCount> Threads{};
		/** The running thread's pc while it runs. */
		uint16 Pc = 0;
		/** The running thread's slot. */
		uint8 Slot = 0;
		/** In the actor it last started a script on and waits for, the slot that script took. */
		uint8 RequestSlot = 0;
		/** A movement's starting point or step. */
		std::array<int32, 3> MoveWork{};
		std::array<uint8, 4> BytesDC{};
		/** The ticks a jump (57) lasts, which Counter counts up to. */
		int16 JumpTicks = 0;
		uint8 DoorStep = 0;
		uint8 PushCount = 0;
		/** The character it represents, FF none. */
		uint16 Character = 0;
		uint16 IdleAnimation = 0;
		uint16 Animation = 0;
		/** FF: chosen by the movement. */
		uint16 AnimationRequest = 0;
		int16 TargetHeight = 0;
		int16 DrawSize = 0;
		int32 WordF0 = 0;
		/** 4096 is 1. */
		std::array<int16, 3> Scale{};
		int16 WordFA = 0;
		/** Two RGB tints, 128 neutral. */
		std::array<uint8, 6> Tints{};
		/** Counts moves, jumps and wandering. */
		int16 Counter = 0;
		/** 4096 a turn; bit 15 set: none (walking) or applied (facing). */
		uint16 MoveDirection = 0;
		uint16 Facing = 0;
		uint16 Yaw = 0;
		uint16 Sound = 0;
		uint8 SoundVolume = 0;
		uint8 SoundMode = 0;
		std::array<uint8, 6> Bytes10E{};
		uint16 LockedFacing = 0;
		uint16 TurnSpeed = 0;
		int16 ExternalAnimationFile = 0;
		uint8 ModelId = 0;
		uint8 ModelIndex = 0;
		/** The Gear joint it rides, FFFF none. */
		uint16 GearJoint = 0;
		uint16 Word12A = 0;
		/**
		 * Bits 0-1: the extra rotation's axis; 2-4: portrait cache slot; 5: a door moving; 6-8: call
		 * depth; 9-11: facing sector 0-7; 12: movement bounds allocated; 13-15: Gear model slot; 16-17:
		 * texture mode; 18-27: the sprite's texture position.
		 */
		uint32 Status = 0;
		/** The sprite's texture settings. */
		uint32 SpriteTexture = 0;
		/** The sprite's drawing settings: bits 5-6 the draw mode; bit 7: Ride holds a platform. */
		uint32 SpriteDraw = 0;

		/**
		 * What the actor remembers of the platform it rides, from its first ride on (in the original a
		 * block kept beside the state): the platform's rotation when last seen, and the distance between
		 * the two, at which it is carried round as the platform turns.
		 */
		struct FRide
		{
			std::array<int16, 3> PlatformRotation{};
			int16 Distance = 0;
		};
		FRide Ride;
		/** The quad (x, z of four corners) the actor cannot leave while Status bit 12 is set. */
		std::array<int16, 8> MoveBounds{};
		/**
		 * The weights its scripts give its model's MIMe animation (in the original a block beside the
		 * state, made for a model with one): key k moves its vertices by entry k, 4096 their whole
		 * deltas. They start at 0.
		 */
		std::array<int32, PartValueCount> PartValues{};

		uint32 CallDepth() const { return (Status & CallDepthMask) >> CallDepthShift; }
		void SetCallDepth(uint32 Depth) { Status = (Status & ~CallDepthMask) | (Depth & 7) << CallDepthShift; }

		FEventThread& Running() { return Threads[Slot]; }

		/**
		 * The defaults the field gives a script actor when it places it, except its floor, which the
		 * walkmesh finds; Counter takes RandomNumber, the next number of the game's generator.
		 */
		WELTALLCORE_API void Reset(int32 RandomNumber);
	};
}
