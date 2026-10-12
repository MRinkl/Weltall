#include "Field/FieldCollisions.h"

#include "EventOps.h"
#include "EventSprites.h"
#include "Field/FieldWalkmesh.h"
#include "MovementRules.h"

#include "Field/FieldActorBox.h"
#include "Field/FieldActorMotion.h"
#include "Field/FieldLayers.h"
#include "Graphics/GteMath.h"
#include "Graphics/Sprite.h"

#include <limits>
#include <string>

namespace Weltall
{
	namespace
	{
		using namespace MovementRules;

		constexpr uint32 Platform = 0x80;
		constexpr uint32 ShapedBody = 0x2000;
		constexpr uint32 NotPushable = 0x10;
		constexpr uint32 OffTheGround = EventActorFlags0::Airborne | EventActorFlags0::FixedHeight;
		/** Flags1 the pass sets on the others: over a platform's polygons, ridden, carrying, met, under the controlled actor. */
		constexpr uint32 OverPlatform = 0x100;
		constexpr uint32 Ridden = 0x4000;
		constexpr uint32 Carrying = 0x8000;
		constexpr uint32 Met = 0x400000;
		constexpr uint32 Below = 0x800000;
		constexpr uint32 PassCleared = OverPlatform | Ridden | Carrying;
		constexpr uint32 NotMet = Met | Below;
		constexpr uint32 RideRecorded = 0x80;
		/** Feet this close above an actor's top stand on it. */
		constexpr int32 StepOnto = 0x10;
		constexpr uint8 PushCountCap = 0x30;
		constexpr uint8 PushCountToMove = 0x20;
		constexpr uint16 LandedSignal = 1;

		int32 QuarterTowardZero(int32 Value)
		{
			return (Value < 0 ? Value + 3 : Value) >> 2;
		}

		/** The other actor stops the controlled one: pushed along by a quarter of its step once pushed long enough, else both stop. */
		void Block(FEventActorState& Leader, FEventActorState& Other, uint32 OtherFlags0)
		{
			if ((OtherFlags0 & NotPushable) == 0)
			{
				if (Other.PushCount < PushCountCap)
				{
					Other.PushCount = static_cast<uint8>(Other.PushCount + 2);
				}
				if (Other.PushCount > PushCountToMove)
				{
					Other.Velocity[0] = Add(Other.Velocity[0], QuarterTowardZero(Leader.Step[0]));
					Other.Velocity[2] = Add(Other.Velocity[2], QuarterTowardZero(Leader.Step[2]));
					Leader.Step = {};
					Leader.Velocity = {};
					return;
				}
			}
			Other.Velocity = {};
			Other.Step = {};
			Leader.Step = {};
			Leader.Velocity = {};
		}

		FError MovementError(const FEventContext& Context, int32 Actor, EEventFault Fault, uint32 Value)
		{
			return MakeError("field map " + std::to_string(Context.RunningMap) + ", actor " + std::to_string(Actor) + " meeting the others: " + EventOps::DescribeFault(Context, Fault, Value));
		}
	}

	std::optional<FError> CollideControlledActor(FEventContext& Context)
	{
		const int32 LeaderIndex = Context.LeaderActor;
		FEventActorState* LeaderState = Context.StateOf(LeaderIndex);
		FSprite* LeaderSprite = EventOps::ActorSprite(Context, LeaderIndex);
		if (LeaderState == nullptr || LeaderSprite == nullptr)
		{
			return MovementError(Context, LeaderIndex, LeaderState == nullptr ? EEventFault::ActorWithoutScripts : EEventFault::NoActorModels, static_cast<uint32>(LeaderIndex));
		}
		FEventActorState& Leader = *LeaderState;
		const int32 X = WholePart(Add(Leader.Position[0], Leader.Step[0]));
		const int32 Z = WholePart(Add(Leader.Position[2], Leader.Step[2]));
		const int32 Feet = WholePart(Leader.Position[1]);
		const int32 Top = Feet - Leader.Height;
		const uint32 LeaderFlags0 = Leader.Flags0;
		const bool bOffTheGround = (LeaderFlags0 & OffTheGround) != 0;
		const uint8 RiddenBefore = Leader.LinkedActor;
		int32 FloorLimit = std::numeric_limits<int32>::max();
		bool bStoodOn = false;
		bool bRides = false;

		for (int32 Index = 0; Index < static_cast<int32>(Context.States.size()); ++Index)
		{
			FEventActorState& Other = Context.States[static_cast<std::size_t>(Index)];
			if (Index == LeaderIndex || (Other.Flags0 & EventActorFlags0::Frozen) != 0)
			{
				continue;
			}
			const uint32 OtherFlags0 = Other.Flags0;
			const bool bPlatform = (Other.Flags1 & Platform) != 0;
			Other.Flags1 &= ~PassCleared;
			int32 Bottom = 0;
			int32 OtherTop = 0;
			bool bRiding = false;
			if (bPlatform)
			{
				const std::optional<FPlatformFloor> Floor = PlatformFloor(Context, Index, X, Z);
				if (Context.Run.Fault != EEventFault::None)
				{
					const EEventFault Fault = Context.Run.Fault;
					Context.Run.Fault = EEventFault::None;
					return MovementError(Context, LeaderIndex, Fault, Context.Run.FaultValue);
				}
				if (!Floor)
				{
					Other.Flags1 &= ~NotMet;
					continue;
				}
				Other.Flags1 |= OverPlatform;
				OtherTop = Floor->Height;
				Bottom = Floor->Height + Other.Height;
				if (Leader.LinkedActor == Index)
				{
					Leader.FloorNormal = Floor->Normal;
					Other.Flags1 |= Ridden;
					bRiding = !bOffTheGround;
				}
			}
			else
			{
				bool bMet = false;
				if ((OtherFlags0 & ShapedBody) != 0)
				{
					bMet = IsInActorBox(Other, X, Z, 0);
				}
				else
				{
					const int32 Across = WholePart(Add(Other.Position[0], Other.Step[0])) - X;
					const int32 Along = WholePart(Add(Other.Position[2], Other.Step[2])) - Z;
					const int32 Reach = Leader.Radius + Other.Radius;
					bMet = Add(GteSquare(Across), GteSquare(Along)) < GteSquare(Reach);
				}
				if (!bMet)
				{
					Other.Flags1 &= ~NotMet;
					continue;
				}
				if ((Leader.FloorAttributes & FieldFloorAttribute::Steep) != 0 || ((OtherFlags0 | LeaderFlags0) & EventActorFlags0::PassesThroughActors) != 0 || Context.bPartyWalking)
				{
					continue;
				}
				Bottom = WholePart(Other.Position[1]);
				OtherTop = Bottom - Other.Height;
				bRiding = Leader.LinkedActor == Index && !bOffTheGround;
			}

			if (bRiding || (Bottom >= Top && Feet >= OtherTop && (Feet < OtherTop + StepOnto || (Other.Flags1 & Below) != 0)))
			{
				FloorLimit = OtherTop;
				Other.Flags1 |= Below;
				Leader.Velocity = Other.Step;
				bStoodOn = true;
				if (!bOffTheGround)
				{
					Leader.LinkedActor = static_cast<uint8>(Index);
					bRides = true;
				}
			}
			else if (Bottom < Top || Feet < OtherTop)
			{
				Other.Flags1 &= ~OverPlatform;
				if (Feet < OtherTop)
				{
					Other.Flags1 |= Below;
					FloorLimit = std::min(FloorLimit, OtherTop);
				}
				else
				{
					Other.Flags1 &= ~Below;
				}
			}
			else
			{
				Block(Leader, Other, OtherFlags0);
			}
			Other.Flags1 |= Met;
		}

		if (!bRides)
		{
			Leader.LinkedActor = FEventContext::NoActor;
		}
		else
		{
			const int32 Carrier = Leader.LinkedActor;
			Context.States[static_cast<std::size_t>(Carrier)].Flags1 |= Carrying;
			if (RiddenBefore == FEventContext::NoActor)
			{
				// The original sets aside the block here the first time; the port keeps it in the state.
				Leader.SpriteDraw |= RideRecorded;
				Leader.Ride.PlatformRotation = Context.Actors[static_cast<std::size_t>(Carrier)].Rotation;
				Leader.Ride.Distance = static_cast<int16>(ActorGroundDistance(Context, LeaderIndex, Carrier));
			}
		}
		if ((Leader.Flags0 & EventActorFlags0::MovedByScript) == 0 && (Leader.Flags1 & EventActorFlags1::ExplicitPosition) == 0)
		{
			if (!StepWalkLayer(Context, LeaderIndex, bStoodOn ? std::optional<int32>(FloorLimit) : std::nullopt))
			{
				return MovementError(Context, LeaderIndex, EEventFault::NoFloorTriangle, static_cast<uint16>(Leader.Layer));
			}
		}
		if (LeaderSprite->Extra.Signal == LandedSignal)
		{
			Context.bPadInputDropped = true;
			Leader.Flags0 &= ~EventActorFlags0::Airborne;
		}
		return std::nullopt;
	}
}
