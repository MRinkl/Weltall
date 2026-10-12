#include "Field/FieldActorMotion.h"

#include "EventOps.h"
#include "EventSprites.h"
#include "Field/FieldWalkmesh.h"
#include "MovementRules.h"

#include "Field/FieldCamera.h"
#include "Graphics/Gte.h"
#include "Graphics/Sprite.h"
#include "Graphics/SpriteSystem.h"

namespace Weltall
{
	namespace
	{
		using namespace MovementRules;

		constexpr int32 NoAnimationChosen = 0xFF;
		/** The only animation that keeps a jump going outside the player's action mode. */
		constexpr int32 JumpingAnimation = 3;
		constexpr uint32 Standing = 0x8000;
		/** Walking goes Pace / the actor's speed divisor, a gear twice that; a sprite moving by its velocity has Speed / the divisor as its speed. */
		constexpr int32 WalkPace = 0x40000;
		constexpr int32 GearPace = 0x80000;
		constexpr int32 SpriteSpeed = 0x4000000;
		/** The walking velocity keeps whole 16ths of a unit. */
		constexpr int32 VelocityGrain = ~0xFFF;
		constexpr uint32 PlacedOrInTheAir = EventActorFlags0::FollowsFloor | EventActorFlags0::Falling | EventActorFlags0::Airborne;

		/** A gear's animation n: the first 16 through the field's table, the others n - 16; the field keeps it to play again. */
		void PlayGearAnimation(FEventContext& Context, int32 Actor, const FEventActorState& State, int32 Animation)
		{
			constexpr std::array<uint8, 16> GearAnimations = {1, 2, 2, 2, 0, 0, 0, 6, 0, 0, 0, 0, 4, 0, 8, 0};
			if (Animation < 0)
			{
				EventOps::Fail(Context, EEventFault::GearModule, static_cast<uint32>(Actor));
				return;
			}
			const int32 Played = Animation < static_cast<int32>(GearAnimations.size()) ? GearAnimations[static_cast<std::size_t>(Animation)] : Animation - 16;
			const int32 Slot = FieldGearSlot(State);
			if (!Context.Gears.Models || Context.Gears.Models->PlayAnimation(Slot, 0, Played))
			{
				EventOps::Fail(Context, EEventFault::GearModule, static_cast<uint32>(Actor));
				return;
			}
			if (Slot >= FFieldGears::Capacity)
			{
				EventOps::Fail(Context, EEventFault::GearArraysFull, static_cast<uint32>(Slot));
				return;
			}
			Context.Gears.Animations[static_cast<std::size_t>(Slot)] = static_cast<int16>(Played);
		}

		/** Velocity along the direction at a pace of Pace / the actor's speed divisor, times the actor's scale. */
		void WalkAtPace(FSprite& Sprite, const FEventActorState& State, int32 Pace, int32 Direction)
		{
			const int32 Step = (ProcessorDivide(Pace, State.SpeedDivisor) >> 8) * 32;
			const int32 Angle = Direction & 0xFFF;
			Sprite.Velocity[0] = Multiply(Multiply(GteCos(Angle), Step) >> 12, State.Scale[0]);
			Sprite.Velocity[2] = Multiply(Wrap(-int64{Multiply(GteSin(Angle), Step)}) >> 12, State.Scale[2]);
		}
	}

	void PlayActorAnimation(FEventContext& Context, int32 Actor, int32 Animation)
	{
		if ((Context.Actors[static_cast<std::size_t>(Actor)].Flags & FieldActorFlag::NoModel) == 0)
		{
			return;
		}
		FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
		if (Animation != JumpingAnimation && Context.Movement.ActionMode == 0)
		{
			State.Flags0 &= ~EventActorFlags0::Airborne;
		}
		if (Animation == NoAnimationChosen)
		{
			Animation = 0;
		}
		if (Animation != Context.Movement.JumpAnimation)
		{
			State.Flags0 &= ~EventActorFlags0::Airborne;
		}
		if ((State.Flags1 & EventActorFlags1::Gear) != 0)
		{
			PlayGearAnimation(Context, Actor, State, Animation);
			return;
		}
		if ((State.Flags1 & EventActorFlags1::NoAnimations) != 0)
		{
			return;
		}
		if (FSprite* Sprite = EventOps::ActorSprite(Context, Actor))
		{
			Context.Models->SpriteSystem()->SetAnimation(*Sprite, Animation);
		}
	}

	void SetActorWalkVelocity(FEventContext& Context, int32 Actor, int32 Direction)
	{
		FSprite* Sprite = EventOps::ActorSprite(Context, Actor);
		if (Sprite == nullptr)
		{
			return;
		}
		const FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
		const bool bModel = (Context.Actors[static_cast<std::size_t>(Actor)].Flags & FieldActorFlag::NoModel) == 0;
		if ((Direction & Standing) != 0)
		{
			Sprite->Velocity[0] = 0;
			Sprite->Velocity[2] = 0;
		}
		else if (bModel)
		{
			WalkAtPace(*Sprite, State, WalkPace, Direction);
		}
		else if ((State.Flags1 & EventActorFlags1::Gear) != 0)
		{
			if ((State.Flags1 & EventActorFlags1::GearMovesIt) != 0)
			{
				// The gear walks as its model's root moved last tick, in whole units.
				const FGearModel* Model = FieldGearModel(Context, FieldGearSlot(State));
				if (Model == nullptr)
				{
					EventOps::Fail(Context, EEventFault::GearModule, static_cast<uint32>(Actor));
					return;
				}
				Sprite->Velocity[0] = Wrap(-int64{Model->RootMotion[0]} * 0x10000);
				Sprite->Velocity[2] = Wrap(-int64{Model->RootMotion[2]} * 0x10000);
				return;
			}
			WalkAtPace(*Sprite, State, GearPace, Direction);
		}
		else if ((State.Flags1 & EventActorFlags1::MovesBySpriteVelocity) != 0)
		{
			WalkAtPace(*Sprite, State, WalkPace, Direction);
			Sprite->Speed = ProcessorDivide(SpriteSpeed, State.SpeedDivisor);
		}
		else
		{
			Context.Models->SpriteSystem()->SetDirection(*Sprite, static_cast<int16>(Direction));
		}
		Sprite->Velocity[0] &= VelocityGrain;
		Sprite->Velocity[2] &= VelocityGrain;
	}

	bool IsActorSettled(const FEventActorState& State, const FFieldMovement& Movement)
	{
		const uint32 LayerBit = State.Layer < 3 ? 1u << State.Layer : 0;
		return (State.FloorAttributes & FieldFloorAttribute::Slopes) == 0 && State.Step == std::array<int32, 3>{} && Movement.bFirstTickDone
			&& State.LinkedActor == FEventContext::NoActor && (State.Flags0 & PlacedOrInTheAir) == 0
			&& (State.Flags1 & EventActorFlags1::LayersIgnored & LayerBit) == 0;
	}

	int32 ActorGroundDistance(const FEventContext& Context, int32 From, int32 To)
	{
		const FEventActorState& A = Context.States[static_cast<std::size_t>(From)];
		const FEventActorState& B = Context.States[static_cast<std::size_t>(To)];
		return FieldLength2D(WholePart(B.Position[0]) - WholePart(A.Position[0]), WholePart(B.Position[2]) - WholePart(A.Position[2]));
	}
}
