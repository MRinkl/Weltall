#include "EventMoves.h"
#include "MovementRules.h"

#include "Field/FieldActorMotion.h"
#include "Graphics/Gte.h"
#include "Graphics/GteMath.h"
#include "Graphics/Sprite.h"

#include <array>

namespace Weltall::EventOps
{
	namespace
	{
		using namespace Moves;
		using MovementRules::WholePart;

		constexpr int32 WalkAnimation = 1;
		constexpr uint16 NoAnimationRequest = 0xFF;
		/** The setup part's length: the instruction's second part follows it. */
		constexpr int32 SetupLength = 9;

		int32 Wrapped(uint32 Value)
		{
			return static_cast<int32>(Value);
		}

		/** The point the setup part names, in 16.16: x, z and y at 2, 4 and 6, its flags byte at 8. */
		std::array<int32, 3> MoveTarget(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 8);
			const int32 X = ReadOperand(Context, 2, Flags, 0x80);
			const int32 Z = ReadOperand(Context, 4, Flags, 0x40);
			const int32 Y = ReadOperand(Context, 6, Flags, 0x20);
			return {Wrapped(static_cast<uint32>(X) << 16), Wrapped(static_cast<uint32>(Y) << 16), Wrapped(static_cast<uint32>(Z) << 16)};
		}

		/**
		 * The setup part: the move's ticks (its length over the stride, at least 1 in 16 bits), the
		 * step per tick toward the point, and the actor turned toward it unless it stands at the
		 * point's x and z. The second part runs in the same tick.
		 */
		bool SetUp(FEventContext& Context, int32 Stride)
		{
			FEventActorState& State = Self(Context);
			const std::array<int32, 3> Target = MoveTarget(Context);
			std::array<int32, 3> Delta{};
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Delta[Axis] = Wrapped(static_cast<uint32>(Target[Axis]) - static_cast<uint32>(State.Position[Axis]));
			}
			int32 Distance = 0;
			if (!Length(Context, Delta[0] >> 16, Delta[1] >> 16, Delta[2] >> 16, Distance))
			{
				return false;
			}
			const int32 Ticks = ProcessorDivide(Distance, Stride);
			State.Counter = static_cast<int16>((Ticks & 0xFFFF) == 0 ? Ticks + 1 : Ticks);
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				State.MoveWork[Axis] = ProcessorDivide(Delta[Axis], State.Counter);
			}
			if (Target[0] >> 16 != WholePart(State.Position[0]) || Target[2] >> 16 != WholePart(State.Position[2]))
			{
				// Not masked to a turn: a heading past half a turn keeps its high bits.
				const int32 Angle = GteRatan2(State.MoveWork[2] >> 16, WholePart(State.MoveWork[0]), *Context.Arctangents);
				State.MoveDirection = static_cast<uint16>(-Angle);
				State.Facing = State.MoveDirection;
			}
			Advance(Context, SetupLength);
			return true;
		}

		/**
		 * The second part, each tick: a step, or when the ticks or the countdown ran out the end (at
		 * the point exactly unless the countdown stopped it), the pc past both parts. The actor's
		 * record and sprite follow it. Returns the animation the move asks for.
		 */
		int32 Continue(FEventContext& Context, FSprite& Sprite, bool bTimed)
		{
			FEventActorState& State = Self(Context);
			FEventThread& Thread = State.Running();
			int32 Animation = WalkAnimation;
			if (State.Counter < 1 || Countdown(Thread) == 0)
			{
				Advance(Context, -SetupLength);
				if (Countdown(Thread) != 0)
				{
					const std::array<int32, 3> Before = State.Position;
					State.Position = MoveTarget(Context);
					for (std::size_t Axis = 0; Axis < 3; ++Axis)
					{
						State.Step[Axis] = Wrapped(static_cast<uint32>(State.Position[Axis]) - static_cast<uint32>(Before[Axis]));
					}
				}
				Animation = static_cast<int16>(State.IdleAnimation);
				Advance(Context, bTimed ? 13 : 11);
				SetCountdown(Thread, NoCountdown);
			}
			else
			{
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					State.Position[Axis] = Wrapped(static_cast<uint32>(State.Position[Axis]) + static_cast<uint32>(State.MoveWork[Axis]));
				}
				State.Step = State.MoveWork;
				Context.Run.bYield = true;
				SetCountdown(Thread, Countdown(Thread) - 1u);
			}
			State.Counter = static_cast<int16>(State.Counter - 1);
			FFieldActor& Record = Context.Actors[static_cast<std::size_t>(Context.Run.Actor)];
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Record.Translation[Axis] = WholePart(State.Position[Axis]);
			}
			Sprite.Position = State.Position;
			return Animation;
		}

		/**
		 * 10 and 11: moves the actor to a point in a straight line at the scripts' pace, taking its
		 * position into its own hands. The instruction has two parts: the setup (its second byte 0),
		 * then a second part (the opcode again, a nonzero byte, and 11's time limit) that runs every
		 * tick until the move is over.
		 */
		template <bool bTimed>
		void ScriptMove(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			if constexpr (bTimed)
			{
				ArmCountdown(Context, 0xB);
			}
			else
			{
				SetCountdown(State.Running(), NoCountdown);
			}
			FSprite* Sprite = MovedSprite(Context);
			if (Sprite == nullptr)
			{
				return;
			}
			int32 Stride = Pace(Context) >> 16;
			if (Stride == 0)
			{
				Stride = 1;
			}
			State.Flags0 |= EventActorFlags0::MovedByScript;
			int32 Animation = WalkAnimation;
			if (CodeByte(Context, 1) == 0)
			{
				if (!SetUp(Context, Stride))
				{
					return;
				}
			}
			else
			{
				Animation = Continue(Context, *Sprite, bTimed);
			}
			if (static_cast<int16>(State.AnimationRequest) != NoAnimationRequest)
			{
				Animation = static_cast<int16>(State.AnimationRequest);
			}
			if (static_cast<int16>(State.Animation) != Animation && (State.Flags0 & EventActorFlags0::AnimationLocked) == 0)
			{
				State.Animation = static_cast<uint16>(Animation);
				PlayActorAnimation(Context, Context.Run.Actor, Animation);
			}
			SetActorWalkVelocity(Context, Context.Run.Actor, static_cast<int16>(State.MoveDirection));
		}
	}

	void RegisterScriptMoves(FBaseTable& Base, FExtendedTable&)
	{
		Base[0x10] = &ScriptMove<false>;
		Base[0x11] = &ScriptMove<true>;
	}
}
