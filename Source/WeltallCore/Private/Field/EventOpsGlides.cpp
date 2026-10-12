#include "EventMoves.h"
#include "MovementRules.h"

#include "Graphics/Gte.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"
#include "Graphics/Sprite.h"

#include <array>
#include <optional>

namespace Weltall::EventOps
{
	namespace
	{
		using namespace Moves;
		using MovementRules::Add;
		using MovementRules::Multiply;
		using MovementRules::WholePart;

		/** Where a glide heads, in whole units; Reach adds to the distance it ends within. */
		struct FGlideTarget
		{
			std::array<int32, 3> Point{};
			int32 Reach = 0;
		};

		/** The point the thread's stage names, or nothing when the move ends at once (an empty party slot) or faults. */
		std::optional<FGlideTarget> GlideTarget(FEventContext& Context, bool& bOverAtOnce)
		{
			FEventActorState& State = Self(Context);
			FGlideTarget Target;
			std::array<int32, 3>& Point = Target.Point;
			switch (Stage(State.Running()))
			{
				case EStage::Point:
				case EStage::FromStart:
				{
					const uint8 Flags = CodeByte(Context, 5);
					Point = {ReadOperand(Context, 1, Flags, 0x80), ReadOperand(Context, 6, Flags, 0x20), ReadOperand(Context, 3, Flags, 0x40)};
					if (Stage(State.Running()) == EStage::FromStart)
					{
						for (std::size_t Axis = 0; Axis < 3; ++Axis)
						{
							Point[Axis] += State.MoveWork[Axis];
						}
					}
					break;
				}
				case EStage::Actor:
				{
					const uint32 Actor = ReadActor(Context, 1);
					if (Actor == FEventContext::NoActor)
					{
						bOverAtOnce = true;
						return std::nullopt;
					}
					const FEventActorState* Other = StateOf(Context, Actor);
					if (Other == nullptr)
					{
						return std::nullopt;
					}
					Target.Reach = RootOfSquare(Other->Radius + State.Radius);
					// The instruction took the actor's ground point; the height is its own operand.
					Point = {State.MoveWork[0], ReadOperand(Context, 2, CodeByte(Context, 4), 0x80), State.MoveWork[2]};
					break;
				}
				case EStage::Direction:
				{
					const int32 Angle = ReadValue(Context, 1) & 0xFFF;
					// The start's x is shifted with the offset, so the glide's x ends near start / 4096.
					Point[0] = Add(State.MoveWork[0], GteCos(Angle) * 0x20) >> 12;
					Point[2] = State.MoveWork[2] + (GteSin(Angle) * -0x20 >> 12);
					Point[1] = ReadOperand(Context, 3, CodeByte(Context, 7), 0x80) + State.MoveWork[1];
					break;
				}
			}
			return Target;
		}

		/**
		 * One tick of a glide toward what the thread's stage names: the actor's velocity set toward it
		 * at the scripts' pace (its height held at where the glide takes it), which the actors'
		 * movement applies. True when the glide is over (within a stride, or its countdown ran out):
		 * the thread's move is reset and the actor faces its way or keeps its facing. False while it
		 * glides on, and the actor yields. A glide whose velocity has no whole part across does not
		 * turn the actor.
		 */
		bool GlideStep(FEventContext& Context, bool bEndsFacingWay)
		{
			FEventActorState& State = Self(Context);
			FSprite* Sprite = MovedSprite(Context);
			if (Sprite == nullptr)
			{
				return false;
			}
			Sprite->Speed = Pace(Context);
			const int32 Reach = RootOfSquare(Sprite->Speed >> 15) + 1;
			bool bOverAtOnce = false;
			const std::optional<FGlideTarget> Target = GlideTarget(Context, bOverAtOnce);
			if (!Target)
			{
				return bOverAtOnce;
			}
			const std::array<int32, 3> From = {WholePart(State.Position[0]), WholePart(State.Position[1]), WholePart(State.Position[2])};
			const FGteLongVector Away = {From[0] - Target->Point[0], From[1] - Target->Point[1], From[2] - Target->Point[2]};
			// libgte adds the squares with a trapping add: a sum past 31 bits would stop the console.
			const int64 Squares = int64{GteSquare(Away[0])} + GteSquare(Away[1]) + GteSquare(Away[2]);
			if (Squares > 0x7FFFFFFF)
			{
				Fail(Context, EEventFault::SquareRootOutsideTable, static_cast<uint32>(Squares));
				return false;
			}
			const FGteLongVector Normal = VectorNormal(Away);
			const int32 Speed = Sprite->Speed >> 8;
			std::array<int32, 3> Velocity{};
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Velocity[Axis] = Multiply(-1, Multiply(Normal[Axis], Speed) >> 4);
			}
			State.Velocity = {Velocity[0], 0, Velocity[2]};
			int32 Distance = 0;
			if (!Length(Context, Target->Point[0] - From[0], Target->Point[1] - From[1], Target->Point[2] - From[2], Distance))
			{
				return false;
			}
			const bool bTurns = (Velocity[0] >> 16) != 0 || (Velocity[2] >> 16) != 0;
			const uint32 FlagsBefore = State.Flags0;
			State.Flags0 |= EventActorFlags0::FollowsFloor;
			const uint16 Heading = static_cast<uint16>(DirectionOf(Context, Velocity[0], Velocity[2]) | DirectionApplied);
			const int16 Height = static_cast<int16>(static_cast<uint32>(Add(State.Position[1], Velocity[1])) >> 16);
			FEventThread& Thread = State.Running();
			const uint16 Left = Countdown(Thread);
			if (Left == 0 || Distance <= Reach + Target->Reach)
			{
				if (bTurns)
				{
					const uint16 Facing = bEndsFacingWay ? Heading : KeptFacing(State, FlagsBefore);
					State.MoveDirection = Facing;
					State.Facing = Facing;
				}
				State.TargetHeight = Height;
				SetStage(Thread, EStage::Point);
				SetCountdown(Thread, NoCountdown);
				return true;
			}
			if (bTurns)
			{
				State.MoveDirection = Heading;
				State.Facing = Heading;
			}
			State.TargetHeight = Height;
			State.Flags0 |= EventActorFlags0::FixedHeight;
			Context.Run.bYield = true;
			SetCountdown(Thread, Left - 1u);
			return false;
		}

		/** Latches an actor's whole position as the glide's ground point: the record the operand's own byte names. */
		bool LatchActorPoint(FEventContext& Context)
		{
			// The byte indexes the records directly: a party alias would read past them.
			const uint8 Actor = CodeByte(Context, 1);
			const FEventActorState* Other = Actor < Context.Actors.size() ? Context.StateOf(Actor) : nullptr;
			if (Other == nullptr)
			{
				Fail(Context, EEventFault::ActorWithoutScripts, Actor);
				return false;
			}
			FEventActorState& State = Self(Context);
			State.MoveWork = {WholePart(Other->Position[0]), WholePart(Other->Position[1]), WholePart(Other->Position[2])};
			return true;
		}

		/** Runs a glide of the given stage without a time limit, and moves past the instruction when it is over. */
		template <EStage GlideStage, int32 Length>
		void Glide(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			if constexpr (GlideStage == EStage::FromStart)
			{
				LatchStart(State, GlideStage);
			}
			else if constexpr (GlideStage == EStage::Actor)
			{
				SetStage(State.Running(), GlideStage);
				if (!LatchActorPoint(Context))
				{
					return;
				}
			}
			SetCountdown(State.Running(), NoCountdown);
			if (GlideStep(Context, false))
			{
				Advance(Context, Length);
			}
		}

		/** Runs a glide of the given stage with the time limit at operand Limit; a limit of 0 also has it end facing its way. */
		template <EStage GlideStage, int32 Limit, int32 Length>
		void TimedGlide(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			if constexpr (GlideStage == EStage::FromStart || GlideStage == EStage::Direction)
			{
				LatchStart(State, GlideStage);
			}
			else if constexpr (GlideStage == EStage::Actor)
			{
				SetStage(State.Running(), GlideStage);
				if (!LatchActorPoint(Context))
				{
					return;
				}
			}
			ArmCountdown(Context, Limit);
			if (GlideStep(Context, ReadValue(Context, Limit) == 0))
			{
				Advance(Context, Length);
			}
		}
	}

	void RegisterGlides(FBaseTable& Base, FExtendedTable&)
	{
		// 4C and 4D leave the thread's stage as it is: a point, unless a move was cut short.
		Base[0x45] = &TimedGlide<EStage::Direction, 5, 8>;
		Base[0x4C] = &Glide<EStage::Point, 8>;
		Base[0x4D] = &TimedGlide<EStage::Point, 8, 10>;
		Base[0x50] = &Glide<EStage::FromStart, 8>;
		Base[0x51] = &TimedGlide<EStage::FromStart, 8, 10>;
		Base[0x54] = &Glide<EStage::Actor, 5>;
		Base[0x55] = &TimedGlide<EStage::Actor, 5, 7>;
	}
}
