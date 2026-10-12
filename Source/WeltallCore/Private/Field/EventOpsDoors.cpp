#include "EventOps.h"

#include "Graphics/Gte.h"
#include "MovementRules.h"

namespace Weltall::EventOps
{
	namespace
	{
		using MovementRules::Wrap;

		/** Status bit 5: a door's swing or slide is under way. */
		constexpr uint32 DoorMoving = 0x20;
		/** The creak the doors start with, on the scripts' channel 3. */
		constexpr int32 DoorSound = 8;
		constexpr int32 DoorSoundChannel = 3;
		/** A swing turns the door by 20h thirty times: about 84 degrees. */
		constexpr int32 SwingSteps = 31;
		constexpr int16 SwingStep = 0x20;
		/** A slide's direction that moves the door up or down instead of along the floor. */
		constexpr int32 SlideUp = 0x1000;
		constexpr int32 SlideDown = 0x1001;
		constexpr int32 QuarterTurn = 0x400;

		FFieldActor& Record(FEventContext& Context)
		{
			return Context.Actors[static_cast<std::size_t>(Context.Run.Actor)];
		}

		/** The move's first call: the door is marked moving and creaks; the instruction runs again. */
		void StartDoor(FEventContext& Context, FEventActorState& State)
		{
			State.Status |= DoorMoving;
			State.DoorStep = 0;
			PlayScriptSound(Context, DoorSound, DoorSoundChannel);
		}

		/** The move's end: the door is open (or closed) and still, and the instruction is passed. */
		void EndDoor(FEventContext& Context, FEventActorState& State, bool bOpen, int32 Length)
		{
			State.DoorStep = 0;
			State.Flags0 = bOpen ? State.Flags0 | EventActorFlags0::DoorOpen : State.Flags0 & ~EventActorFlags0::DoorOpen;
			State.Status &= ~DoorMoving;
			Advance(Context, Length);
		}

		/**
		 * C4, C5: swings the door open or shut about its y axis, a step each time the instruction
		 * runs. It never ends the actor's turn, so the swing takes as many steps a tick as the budget
		 * allows. The byte turns it the other way: open is +20h a step for 0, shut the reverse.
		 */
		template <bool bOpens>
		void SwingDoor(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			if (((State.Flags0 & EventActorFlags0::DoorOpen) != 0) == bOpens)
			{
				Advance(Context, 2);
			}
			else if ((State.Status & DoorMoving) == 0)
			{
				StartDoor(Context, State);
			}
			else if (++State.DoorStep < SwingSteps)
			{
				const int16 Step = (CodeByte(Context, 1) == 0) == bOpens ? SwingStep : static_cast<int16>(-SwingStep);
				int16& Yaw = Record(Context).Rotation[1];
				Yaw = static_cast<int16>(Yaw + Step);
			}
			else
			{
				EndDoor(Context, State, bOpens, 2);
			}
			BuildModelMatrix(Context, static_cast<uint32>(Context.Run.Actor));
		}

		/**
		 * E8, E9: slides the door's model (its translation, not the actor's position) for a number of
		 * steps at a speed: up or down, or along the floor at an angle from the door's yaw. The opening
		 * starts from the actor's position; the closing goes back from where it ended, along the floor
		 * only (up and down move the same way both times). A vertical step sets the height from the
		 * working position's z, as the original does. Like the swings it never ends the actor's turn.
		 */
		template <bool bOpens>
		void SlideDoor(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			if (((State.Flags0 & EventActorFlags0::DoorOpen) != 0) == bOpens)
			{
				Advance(Context, 7);
			}
			else if ((State.Status & DoorMoving) == 0)
			{
				StartDoor(Context, State);
				if (bOpens)
				{
					State.MoveWork = State.Position;
				}
			}
			else if (static_cast<int32>(++State.DoorStep) < ReadValue(Context, 3))
			{
				FFieldActor& Door = Record(Context);
				std::array<int32, 3>& Work = State.MoveWork;
				const int32 Direction = ReadValue(Context, 5);
				if (Direction == SlideUp || Direction == SlideDown)
				{
					const int32 Speed = ReadValue(Context, 1);
					const int16 Height = static_cast<int16>(Work[2] >> 16);
					Work[1] = Wrap(int64{Work[1]} + int64{Speed} * (Direction == SlideUp ? -0x10 : 0x10));
					Door.Translation[1] = Height;
				}
				else
				{
					const int32 Angle = Door.Rotation[1] + ReadValue(Context, 5) - QuarterTurn;
					const int64 Sign = bOpens ? 1 : -1;
					Work[0] = Wrap(int64{Work[0]} + Sign * GteCos(Angle) * ReadValue(Context, 1));
					Work[2] = Wrap(int64{Work[2]} - Sign * GteSin(Angle) * ReadValue(Context, 1));
					Door.Translation[0] = static_cast<int16>(Work[0] >> 16);
					Door.Translation[2] = static_cast<int16>(Work[2] >> 16);
				}
			}
			else
			{
				EndDoor(Context, State, bOpens, 7);
			}
			BuildModelMatrix(Context, static_cast<uint32>(Context.Run.Actor));
		}
	}

	void RegisterDoors(FBaseTable& Base, FExtendedTable&)
	{
		Base[0xC4] = &SwingDoor<true>;
		Base[0xC5] = &SwingDoor<false>;
		Base[0xE8] = &SlideDoor<true>;
		Base[0xE9] = &SlideDoor<false>;
	}
}
