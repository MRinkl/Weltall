#include "EventMoves.h"
#include "EventSprites.h"

#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"

namespace Weltall::EventOps::Moves
{
	namespace
	{
		constexpr int32 WalkPace = 0x4000000;
		constexpr int32 GearPace = 0x8000000;
		constexpr int32 AngleMask = 0xFFF;
	}

	void LatchStart(FEventActorState& State, EStage Stage)
	{
		FEventThread& Thread = State.Running();
		if (Moves::Stage(Thread) != EStage::Point)
		{
			return;
		}
		SetStage(Thread, Stage);
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			State.MoveWork[Axis] = static_cast<int16>(State.Position[Axis] >> 16);
		}
	}

	void ArmCountdown(FEventContext& Context, int32 Offset)
	{
		FEventThread& Thread = Self(Context).Running();
		if (Countdown(Thread) == NoCountdown)
		{
			SetCountdown(Thread, static_cast<uint32>(ReadValue(Context, Offset)));
		}
	}

	int32 Pace(FEventContext& Context)
	{
		const FEventActorState& State = Self(Context);
		return ProcessorDivide((State.Flags1 & EventActorFlags1::Gear) != 0 ? GearPace : WalkPace, State.SpeedDivisor);
	}

	int32 RootOfSquare(int32 Value)
	{
		return SquareRoot0(GteSquare(Value));
	}

	uint16 DirectionOf(const FEventContext& Context, int32 X, int32 Z)
	{
		return static_cast<uint16>(-GteRatan2(Z, X, *Context.Arctangents) & AngleMask);
	}

	uint16 KeptFacing(const FEventActorState& State, uint32 FlagsBefore)
	{
		const uint16 Facing = (FlagsBefore & EventActorFlags0::FacingLocked) != 0 ? State.LockedFacing : State.Facing;
		return static_cast<uint16>(Facing | DirectionApplied);
	}

	FSprite* MovedSprite(FEventContext& Context)
	{
		FSprite* Sprite = ActorSprite(Context, Context.Run.Actor);
		if (Sprite == nullptr)
		{
			Fail(Context, EEventFault::ActorWithoutSprite, static_cast<uint32>(Context.Run.Actor));
		}
		return Sprite;
	}
}
