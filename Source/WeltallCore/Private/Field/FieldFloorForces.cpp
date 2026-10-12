#include "Field/FieldActorMove.h"

#include "EventOps.h"
#include "EventSprites.h"
#include "Field/FieldWalkmesh.h"
#include "MovementRules.h"

#include "Field/FieldActorMotion.h"
#include "Field/FieldWalkTrace.h"
#include "Graphics/Gte.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"
#include "Graphics/Sprite.h"

#include <array>

namespace Weltall
{
	namespace
	{
		using namespace MovementRules;

		/** A pushing floor's speed (units per tick) by attribute bits 9-10, and its direction by bits 11-13. */
		constexpr std::array<int32, 4> PushSpeeds = {4, 8, 16, 32};
		constexpr std::array<int32, 8> PushDirections = {0xC00, 0xE00, 0, 0x200, 0x400, 0x600, 0x800, 0xA00};
		/** Slides top out at 12 units per tick, 24 on a steep slope. */
		constexpr int32 SlideCap = 0xC;
		constexpr int32 SteepSlideCap = 0x18;
		constexpr uint32 OffTheGround = EventActorFlags0::Airborne | EventActorFlags0::Falling | EventActorFlags0::FixedHeight;
		constexpr uint32 PlatformBits = 0xC0;
		constexpr uint32 RideRecorded = 0x80;
		constexpr uint16 Standing = 0x8000;
		constexpr uint32 GearMovedByModel = EventActorFlags1::Gear | EventActorFlags1::GearMovesIt;

		void AddVelocity(FEventActorState& State, int32 X, int32 Y, int32 Z)
		{
			State.Velocity = {Add(State.Velocity[0], X), Add(State.Velocity[1], Y), Add(State.Velocity[2], Z)};
		}

		/**
		 * The slide down a slope at last tick's slide speed, along the floor normal's fall line, and the
		 * slide speed grown by the sprite's gravity for the next tick (the sprite falls at half of it).
		 */
		std::array<int32, 2> Slide(FEventActorState& State, FSprite& Sprite, uint32 Attributes)
		{
			const std::array<int32, 3>& Normal = State.FloorNormal;
			FGteLongVector Across{Wrap(-int64{Multiply(Normal[0], Normal[1])}) >> 15, 1, Wrap(-int64{Multiply(Normal[2], Normal[1])}) >> 15};
			for (const std::size_t Axis : {std::size_t{0}, std::size_t{2}})
			{
				if (Across[Axis] == 0)
				{
					Across[Axis] = 1;
				}
			}
			FGteLongVector Down = VectorNormal(Across);
			for (int32& Component : Down)
			{
				if (Component == 0)
				{
					Component = 1;
				}
			}
			const int32 Speed = State.WordF0 >> 17;
			const std::array<int32, 2> Sliding{Multiply(Multiply(Down[0], Speed), 16), Multiply(Multiply(Down[2], Speed), 16)};
			const int32 Cap = (Attributes & FieldFloorAttribute::Steep) != 0 ? SteepSlideCap : SlideCap;
			State.WordF0 = (State.WordF0 >> 16) < Cap ? Add(State.WordF0, Sprite.Gravity) : Cap << 16;
			Sprite.Velocity[1] = State.WordF0 >> 1;
			return Sliding;
		}

		/** A rider is carried to where its distance from the platform puts it once the platform's turn is taken. */
		void Carry(FEventContext& Context, int32 Actor, FEventActorState& State)
		{
			const int32 Platform = State.LinkedActor;
			const FEventActorState* Carrier = EventOps::StateOf(Context, static_cast<uint32>(Platform));
			if (Carrier == nullptr || (Carrier->Flags1 & PlatformBits) != PlatformBits)
			{
				return;
			}
			// The original sets aside a new block here when it has none; its first contents are
			// whatever the heap held, and the port keeps what the actor last recorded.
			State.SpriteDraw |= RideRecorded;
			const FFieldActor& Record = Context.Actors[static_cast<std::size_t>(Platform)];
			const int32 Turn = static_cast<int16>(Record.Rotation[1] - State.Ride.PlatformRotation[1]);
			State.Ride.PlatformRotation[1] = Record.Rotation[1];
			if ((State.MoveDirection & Standing) == 0)
			{
				State.Ride.Distance = static_cast<int16>(ActorGroundDistance(Context, Actor, Platform));
			}
			const int32 Distance = State.Ride.Distance;
			const int32 Toward = static_cast<int16>(GteRatan2(Wrap(int64{Carrier->Position[2]} - State.Position[2]), Wrap(int64{Carrier->Position[0]} - State.Position[0]), *Context.Arctangents));
			const int32 Angle = (Toward - Turn - 0x800) & 0xFFF;
			const int32 X = Add(Add(Carrier->Position[0], Multiply(Multiply(GteCos(Angle), Distance), 16)), -int64{State.Position[0]});
			const int32 Z = Add(Add(Carrier->Position[2], Multiply(Multiply(GteSin(Angle), Distance), 16)), -int64{State.Position[2]});
			AddVelocity(State, X, 0, Z);
		}
	}

	void ApplyFloorForces(FEventContext& Context, int32 Actor)
	{
		FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
		FSprite* Sprite = EventOps::ActorSprite(Context, Actor);
		if (Sprite == nullptr)
		{
			EventOps::Fail(Context, EEventFault::NoActorModels, static_cast<uint32>(Actor));
			return;
		}
		const int32 Layer = static_cast<int16>(State.Layer);
		const bool bAttributesSeen = (State.Flags1 >> ((Layer + EventActorFlags1::AttributesIgnoredShift) & 31) & 1) == 0 && !Context.bPartyWalking;
		const uint32 Attributes = bAttributesSeen ? State.FloorAttributes : 0;
		const std::array<int32, 3> Push = FloorPushVelocity(Context.Movement, PushSpeeds[Attributes >> 9 & 3],
			static_cast<int32>((PushDirections[Attributes >> 11 & 7] + Context.Movement.FloorPushAngle) & 0xFFF));

		bool bPushed = false;
		if ((State.Flags0 & OffTheGround) != 0)
		{
			bPushed = (Attributes & FieldFloorAttribute::PushesInTheAir) != 0;
		}
		else
		{
			std::array<int32, 2> Sliding{};
			if ((Attributes & (FieldFloorAttribute::Slope | FieldFloorAttribute::Steep)) != 0)
			{
				Sliding = Slide(State, *Sprite, Attributes);
			}
			if ((Attributes & FieldFloorAttribute::Steep) != 0)
			{
				AddVelocity(State, Sliding[0], 0, Sliding[1]);
				State.MoveDirection |= Standing;
			}
			if (State.LinkedActor == FEventContext::NoActor)
			{
				if ((Attributes & FieldFloorAttribute::Slope) != 0)
				{
					AddVelocity(State, Sliding[0], 0, Sliding[1]);
				}
				bPushed = (Attributes & FieldFloorAttribute::PushesOnTheGround) != 0;
			}
		}
		if (bPushed)
		{
			AddVelocity(State, Push[0], Push[1], Push[2]);
		}
		if (State.LinkedActor != FEventContext::NoActor)
		{
			Carry(Context, Actor, State);
		}
		if ((State.Flags1 & GearMovedByModel) == GearMovedByModel)
		{
			// The gear actors moved by their models take root motion in the order the pass meets them, not by their slots.
			const FGearModel* Model = FieldGearModel(Context, Context.Gears.MetThisTick);
			if (Model == nullptr)
			{
				EventOps::Fail(Context, EEventFault::GearModule, static_cast<uint32>(Actor));
				return;
			}
			const auto RootPush = [&State](int32 Motion) { return Multiply(ProcessorDivide(Wrap(int64{Motion} << 16), State.SpeedDivisor), -0x100); };
			State.Velocity[0] = Add(State.Velocity[0], RootPush(Model->RootMotion[0]));
			++Context.Gears.MetThisTick;
			State.Velocity[2] = Add(State.Velocity[2], RootPush(Model->RootMotion[2]));
		}
	}
}
