#pragma once

#include "EventOps.h"

// What the scripts' walks, glides and moves share: the running thread's countdown and stage, the
// start a relative move takes, the pace, and the library's lengths and headings.
namespace Weltall::EventOps::Moves
{
	/** A countdown of FFFF: none is armed, and a move without a time limit keeps it so. */
	inline constexpr uint16 NoCountdown = 0xFFFF;
	inline constexpr uint16 DirectionApplied = 0x8000;

	/** What a move heads for, the thread's stage: a point, a point from its start, an actor, a direction. */
	enum class EStage : uint32
	{
		Point = 0,
		FromStart = 1,
		Actor = 2,
		Direction = 3,
	};

	inline constexpr uint32 StageShift = 23;

	inline EStage Stage(const FEventThread& Thread)
	{
		return static_cast<EStage>((Thread.Word & FEventThread::MoveStageMask) >> StageShift);
	}

	inline void SetStage(FEventThread& Thread, EStage Stage)
	{
		Thread.Word = (Thread.Word & ~FEventThread::MoveStageMask) | static_cast<uint32>(Stage) << StageShift;
	}

	inline uint16 Countdown(const FEventThread& Thread)
	{
		return static_cast<uint16>(Thread.Word & FEventThread::MoveCounterMask);
	}

	inline void SetCountdown(FEventThread& Thread, uint32 Value)
	{
		Thread.Word = (Thread.Word & ~FEventThread::MoveCounterMask) | (Value & FEventThread::MoveCounterMask);
	}

	/** A move's first tick from where the actor stands: the stage given and the actor's whole position kept. */
	void LatchStart(FEventActorState& State, EStage Stage);

	/** Arms the countdown from the value operand at Offset, unless one is armed. */
	void ArmCountdown(FEventContext& Context, int32 Offset);

	/** The pace scripts move the running actor at, 16.16 per tick: twice as fast for a gear. */
	int32 Pace(FEventContext& Context);

	/** libgte's root of a value's square: the value's size as the table rounds it, from its low 16 bits. */
	int32 RootOfSquare(int32 Value);

	/** The heading of (X, Z), 4096 a turn, as the field turns actors toward a point. */
	uint16 DirectionOf(const FEventContext& Context, int32 X, int32 Z);

	/** The facing a finished move leaves, marked applied: the locked one while the facing is locked. */
	uint16 KeptFacing(const FEventActorState& State, uint32 FlagsBefore);

	/** The running actor's sprite, which scripts' moves drive; null, with a fault, when it has none. */
	FSprite* MovedSprite(FEventContext& Context);
}
