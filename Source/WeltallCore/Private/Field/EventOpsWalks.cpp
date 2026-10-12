#include "EventMoves.h"
#include "MovementRules.h"

#include "Graphics/Gte.h"
#include "Graphics/Sprite.h"

namespace Weltall::EventOps
{
	namespace
	{
		using namespace Moves;
		using EventActorFlags0::WalksToLeader;
		using MovementRules::WholePart;

		/**
		 * One tick of a walk toward what the thread's stage names; the actors' movement does the
		 * walking, at the sprite's speed. True when the walk is over (it arrived within a stride, or
		 * its countdown ran out): the thread's move is reset, and the actor faces the target or keeps
		 * its facing. False while it walks on, turned toward the target, and the actor yields.
		 */
		bool WalkStep(FEventContext& Context, bool bEndsFacingTarget)
		{
			FEventActorState& State = Self(Context);
			FSprite* Sprite = MovedSprite(Context);
			if (Sprite == nullptr)
			{
				return false;
			}
			if ((State.Flags1 & EventActorFlags1::Gear) != 0 || Sprite->Speed == 0)
			{
				Sprite->Speed = Pace(Context);
			}
			const int32 Stride = RootOfSquare(Sprite->Speed >> 15);
			FEventThread& Thread = State.Running();
			const uint8 Flags = CodeByte(Context, 5);
			int32 TargetX = 0;
			int32 TargetZ = 0;
			int32 Reach = 0;
			switch (Stage(Thread))
			{
				case EStage::Point:
					TargetX = ReadOperand(Context, 1, Flags, 0x80);
					TargetZ = ReadOperand(Context, 3, Flags, 0x40);
					break;
				case EStage::FromStart:
					TargetX = ReadOperand(Context, 1, Flags, 0x80) + State.MoveWork[0];
					TargetZ = ReadOperand(Context, 3, Flags, 0x40) + State.MoveWork[2];
					break;
				case EStage::Actor:
				{
					const uint32 Actor = ReadActor(Context, 1);
					if (Actor == FEventContext::NoActor)
					{
						// Over at once, the thread's move left as it is.
						return true;
					}
					const FEventActorState* Other = StateOf(Context, Actor);
					if (Other == nullptr)
					{
						return false;
					}
					Reach = RootOfSquare(Other->Radius + State.Radius);
					TargetX = WholePart(Other->Position[0]);
					TargetZ = WholePart(Other->Position[2]);
					if (CodeByte(Context, 1) == Context.LeaderActor)
					{
						State.Flags0 |= WalksToLeader;
					}
					break;
				}
				case EStage::Direction:
				{
					// A point 4096 units from the start: the walk ends by its countdown.
					const int32 Angle = ReadValue(Context, 1);
					TargetX = State.MoveWork[0] + GteCos(Angle & 0xFFF);
					TargetZ = State.MoveWork[2] - GteSin(Angle & 0xFFF);
					break;
				}
			}
			const int32 DeltaX = TargetX - WholePart(State.Position[0]);
			const int32 DeltaZ = TargetZ - WholePart(State.Position[2]);
			int32 Distance = 0;
			if (!Length(Context, DeltaX, DeltaZ, 0, Distance))
			{
				return false;
			}
			const uint32 FlagsBefore = State.Flags0;
			State.Flags0 |= EventActorFlags0::FollowsFloor;
			const uint16 Left = Countdown(Thread);
			if (Left == 0 || Distance <= Stride + 1 + Reach)
			{
				// Facing the target leaves the direction unapplied, so the actor walks on that way.
				const uint16 Facing = bEndsFacingTarget ? DirectionOf(Context, DeltaX, DeltaZ) : KeptFacing(State, FlagsBefore);
				State.MoveDirection = Facing;
				State.Facing = Facing;
				SetCountdown(Thread, NoCountdown);
				SetStage(Thread, EStage::Point);
				State.Flags0 &= ~(EventActorFlags0::Airborne | WalksToLeader | EventActorFlags0::AnimationLocked);
				return true;
			}
			SetCountdown(Thread, Left - 1u);
			const uint16 Heading = DirectionOf(Context, DeltaX, DeltaZ);
			State.MoveDirection = Heading;
			State.Facing = Heading;
			Context.Run.bYield = true;
			return false;
		}

		/** Runs a walk of the given stage without a time limit, and moves past the instruction when it is over. */
		template <EStage WalkStage, int32 Length>
		void Walk(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			if constexpr (WalkStage == EStage::FromStart)
			{
				LatchStart(State, WalkStage);
			}
			else
			{
				SetStage(State.Running(), WalkStage);
			}
			SetCountdown(State.Running(), NoCountdown);
			if (WalkStep(Context, false))
			{
				Advance(Context, Length);
			}
		}

		/**
		 * Runs a walk of the given stage with the time limit at operand Limit; a limit of 0 also has it
		 * end facing the target.
		 */
		template <EStage WalkStage, int32 Limit, int32 Length>
		void TimedWalk(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			if constexpr (WalkStage == EStage::FromStart || WalkStage == EStage::Direction)
			{
				LatchStart(State, WalkStage);
			}
			else
			{
				SetStage(State.Running(), WalkStage);
			}
			ArmCountdown(Context, Limit);
			if (WalkStep(Context, ReadValue(Context, Limit) == 0))
			{
				Advance(Context, Length);
			}
		}
	}

	void RegisterWalks(FBaseTable& Base, FExtendedTable&)
	{
		Base[0x44] = &TimedWalk<EStage::Direction, 3, 5>;
		Base[0x4A] = &Walk<EStage::Point, 6>;
		Base[0x4B] = &TimedWalk<EStage::Point, 6, 8>;
		Base[0x4E] = &Walk<EStage::FromStart, 6>;
		Base[0x4F] = &TimedWalk<EStage::FromStart, 6, 8>;
		Base[0x52] = &Walk<EStage::Actor, 2>;
		Base[0x53] = &TimedWalk<EStage::Actor, 2, 4>;
	}
}
