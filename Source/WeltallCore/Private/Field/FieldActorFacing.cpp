#include "Field/FieldActorFacing.h"

#include "Field/EventContext.h"
#include "Field/FieldWalkmesh.h"
#include "Graphics/SpriteSystem.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr uint32 HalfTurn = 0x800;
		constexpr uint32 AngleMask = 0xFFF;
		constexpr int32 LadderTurnSpeed = 0x200;
		/** A floor's push direction, bits 11 to 13: its index less 2, in eighths of a turn. */
		constexpr uint32 PushDirectionShift = 11;

		bool Turns(const FFieldActor& Record, const FEventActorState& State)
		{
			constexpr uint16 TakesPart = 0xF40;
			if ((Record.Flags & TakesPart) == 0 || (Record.Flags & FieldActorFlag::Hidden) != 0)
			{
				return false;
			}
			const uint32 Stopped = State.Flags1 & (EventActorFlags1::OffScreen | EventActorFlags1::KeepsRunningOffScreen);
			return (State.Flags1 & EventActorFlags1::Removed) == 0 && Stopped != EventActorFlags1::OffScreen;
		}

		int32 TurnedYaw(const FEventContext& Context, const FEventActorState& State)
		{
			int32 Target = static_cast<int16>(State.Facing);
			int32 Speed = static_cast<int16>((State.Flags1 & EventActorFlags1::Gear) != 0 ? Context.GearTurnSpeed : State.TurnSpeed);
			if ((State.FloorAttributes & FieldFloorAttribute::Ladder) != 0 && (State.Flags0 & EventActorFlags0::InTheAir) == 0)
			{
				Target = static_cast<int32>((((State.FloorAttributes >> PushDirectionShift) - 2) & 7) << 9);
				Speed = LadderTurnSpeed;
			}
			// While the map settles, actors take their facing at once.
			if (Context.SettleTicks != 0)
			{
				return Target & static_cast<int32>(AngleMask);
			}
			return StepFieldAngle(static_cast<int16>(State.Yaw), Target, Speed);
		}
	}

	int32 StepFieldAngle(int32 Angle, int32 Target, int32 Speed)
	{
		const uint32 From = static_cast<uint32>(Angle);
		const uint32 To = static_cast<uint32>(Target);
		const uint32 By = static_cast<uint32>(Speed);
		uint32 Turned = To;
		if (((From - To) & AngleMask) < HalfTurn)
		{
			if (((From - By - To) & AngleMask) < HalfTurn)
			{
				Turned = From - By;
			}
		}
		else if (((From + By - To) & AngleMask) >= HalfTurn)
		{
			Turned = From + By;
		}
		return static_cast<int32>(Turned & AngleMask);
	}

	void TurnFieldActors(FEventContext& Context)
	{
		FSpriteSystem* System = Context.Models != nullptr ? Context.Models->SpriteSystem() : nullptr;
		const std::size_t Count = std::min(Context.States.size(), Context.Actors.size());
		for (std::size_t Actor = 0; Actor < Count; ++Actor)
		{
			FEventActorState& State = Context.States[Actor];
			if (!Turns(Context.Actors[Actor], State))
			{
				continue;
			}
			if ((State.Flags0 & EventActorFlags0::FacingLocked) == 0)
			{
				State.Yaw = static_cast<uint16>(TurnedYaw(Context, State));
			}
			// An actor without a sprite has nothing to turn; the original writes near address 0.
			FSprite* Sprite = System != nullptr ? Context.Models->ActorSprite(static_cast<int32>(Actor)) : nullptr;
			if (Context.bActorSpritesReleased || Sprite == nullptr)
			{
				continue;
			}
			if ((State.Flags1 & EventActorFlags1::NoAnimations) == 0)
			{
				System->SetFacing(*Sprite, static_cast<int16>(Context.Camera.ViewYaw + Context.ViewTurn + State.Yaw));
			}
			else
			{
				System->SetDirection(*Sprite, static_cast<int16>(State.Yaw));
			}
		}
	}
}
