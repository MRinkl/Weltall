#include "EventOps.h"
#include "FieldDirections.h"

#include "Game/PsxRandom.h"
#include "Graphics/GteMath.h"

// Where actors face, how they turn, and how far apart they are.
namespace Weltall::EventOps
{
	namespace
	{
		constexpr uint16 AngleSet = 0x8000;
		/** The directions' table holds the eight twice; AA reads the second copy. */
		constexpr int32 DirectionCount = 8;

		/** The running actor's facing as a direction 0-7. */
		int32 FacingDirection(const FEventActorState& State)
		{
			return FieldDirections::FacingEighth(State.Facing);
		}

		/** Turns an actor: while the map's init runs, its displayed yaw turns at once too. */
		void Face(FEventContext& Context, FEventActorState& State, uint16 Angle)
		{
			const uint16 Set = Angle | AngleSet;
			if (!Context.bInitDone)
			{
				State.Yaw = Set;
			}
			State.MoveDirection = Set;
			State.Facing = Set;
		}

		/** A direction's angle turned into the screen's: counted from the way the camera's goals look. */
		uint16 OnScreen(const FEventContext& Context, uint16 Angle)
		{
			return static_cast<uint16>((Angle - Context.Camera.GoalViewYaw) & 0xFFF);
		}

		/** The angle from one actor to another on the ground, as the field turns actors toward others. */
		uint16 AngleToward(FEventContext& Context, const FEventActorState& From, const FEventActorState& To)
		{
			const int32 Angle = GteRatan2(To.Position[2] - From.Position[2], To.Position[0] - From.Position[0], *Context.Arctangents);
			return static_cast<uint16>(-static_cast<int16>(Angle) | AngleSet);
		}

		void GetFacing(FEventContext& Context)
		{
			SetVar(Context, ReadU16(Context, 1), FacingDirection(Self(Context)));
			Advance(Context, 3);
		}

		/** 5F: faces one of the eight directions by the scripts' own numbering of them. */
		void SetFacingCode(FEventContext& Context)
		{
			static constexpr std::array<uint16, 8> Directions = {6, 2, 4, 0, 5, 7, 3, 1};
			const uint8 Code = CodeByte(Context, 1);
			if (Code >= Directions.size())
			{
				Fail(Context, EEventFault::TableIndexOutOfRange, Code);
				return;
			}
			const uint16 Angle = static_cast<uint16>(Directions[Code] << 9 | AngleSet);
			FEventActorState& State = Self(Context);
			State.MoveDirection = Angle;
			State.Facing = Angle;
			if (!Context.bInitDone)
			{
				State.Yaw = Angle;
			}
			Advance(Context, 2);
		}

		/** 69: faces a direction. */
		void Turn(FEventContext& Context)
		{
			if (const std::optional<uint16> Angle = DirectionAngle(Context, ReadValue(Context, 1)))
			{
				Face(Context, Self(Context), *Angle);
				Advance(Context, 3);
			}
		}

		/** 6A: faces a direction of the screen's. */
		void TurnOnScreen(FEventContext& Context)
		{
			if (const std::optional<uint16> Angle = DirectionAngle(Context, ReadValue(Context, 1)))
			{
				Face(Context, Self(Context), OnScreen(Context, *Angle));
				Advance(Context, 3);
			}
		}

		/** AA: faces a direction of the screen's given as a constant byte. */
		void FaceOnScreen(FEventContext& Context)
		{
			if (const std::optional<uint16> Angle = DirectionAngle(Context, DirectionCount + CodeByte(Context, 1)))
			{
				Face(Context, Self(Context), OnScreen(Context, *Angle));
				Advance(Context, 2);
			}
		}

		/** 6B and 6C: turns by some eighths of a turn from the current facing. */
		template <int32 Sign>
		void TurnBy(FEventContext& Context)
		{
			const int32 Steps = ReadValue(Context, 1);
			const int32 Direction = FacingDirection(Self(Context));
			if (const std::optional<uint16> Angle = DirectionAngle(Context, (Direction + Sign * Steps) & 7))
			{
				Face(Context, Self(Context), *Angle);
				Advance(Context, 3);
			}
		}

		/** 67 and 68: turns another actor to a direction, the world's or the screen's. */
		template <bool bOnScreen>
		void TurnActor(FEventContext& Context)
		{
			const std::optional<uint16> Angle = DirectionAngle(Context, ReadValue(Context, 2));
			if (!Angle)
			{
				return;
			}
			const uint32 Actor = ReadActor(Context, 1);
			if (Actor != FEventContext::NoActor)
			{
				FEventActorState* Target = StateOf(Context, Actor);
				if (Target == nullptr)
				{
					return;
				}
				Face(Context, *Target, bOnScreen ? OnScreen(Context, *Angle) : *Angle);
			}
			Advance(Context, 4);
		}

		/** 6F: faces another actor; the displayed yaw follows in its own time, even during the map's init. */
		void FaceActor(FEventContext& Context)
		{
			const uint32 Actor = ReadActor(Context, 1);
			if (Actor != FEventContext::NoActor)
			{
				const FEventActorState* Target = StateOf(Context, Actor);
				if (Target == nullptr)
				{
					return;
				}
				FEventActorState& State = Self(Context);
				const uint16 Angle = AngleToward(Context, State, *Target);
				State.MoveDirection = Angle;
				State.Facing = Angle;
			}
			Advance(Context, 2);
		}

		/** 70: faces the actor of a party slot, when the slot is filled. */
		void FacePartyMember(FEventContext& Context)
		{
			const uint8 Slot = CodeByte(Context, 1);
			if (Slot >= Context.PartyActors.size())
			{
				Fail(Context, EEventFault::TableIndexOutOfRange, Slot);
				return;
			}
			const uint8 Actor = Context.PartyActors[Slot];
			if (Actor != FEventContext::NoActor)
			{
				const FEventActorState* Target = StateOf(Context, Actor);
				if (Target == nullptr)
				{
					return;
				}
				FEventActorState& State = Self(Context);
				const uint16 Angle = AngleToward(Context, State, *Target);
				State.MoveDirection = Angle;
				State.Facing = Angle;
			}
			Advance(Context, 2);
		}

		/** 89: goes on when the running actor is nearer than the operand to another actor, else jumps. */
		void IfActorWithinDistance(FEventContext& Context)
		{
			const uint32 Actor = ReadActor(Context, 1);
			if (Actor != FEventContext::NoActor)
			{
				const FEventActorState* Target = StateOf(Context, Actor);
				if (Target == nullptr)
				{
					return;
				}
				const FEventActorState& State = Self(Context);
				int32 Distance = 0;
				if (!Length(Context, (State.Position[0] >> 16) - (Target->Position[0] >> 16), (State.Position[1] >> 16) - (Target->Position[1] >> 16),
						(State.Position[2] >> 16) - (Target->Position[2] >> 16), Distance))
				{
					return;
				}
				if (Distance < ReadValue(Context, 2))
				{
					Advance(Context, 6);
					return;
				}
			}
			Self(Context).Pc = ReadU16(Context, 4);
		}

		/** FE 01: wandering: every 16th run walks 45 degrees left or right of the facing, else straight on. */
		void RandomTurn(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			uint16 Direction = State.Facing;
			State.Counter = static_cast<int16>(State.Counter + 1);
			if ((State.Counter & 0xF) == 0)
			{
				Direction = (Context.Random->Next() & 1) == 0 ? static_cast<uint16>(State.Facing + 0x200) : static_cast<uint16>(State.Facing - 0x200);
				Direction &= 0xFFF;
			}
			Context.Run.bYield = true;
			State.MoveDirection = Direction;
			Advance(Context, 1);
		}

		/** FE 17: turns one actor toward another. */
		void TurnToActor(FEventContext& Context)
		{
			const uint32 Turning = ReadActor(Context, 1);
			const uint32 Toward = ReadActor(Context, 2);
			if (Turning != FEventContext::NoActor && Toward != FEventContext::NoActor)
			{
				const FEventActorState* Target = StateOf(Context, Toward);
				FEventActorState* State = StateOf(Context, Turning);
				if (Target == nullptr || State == nullptr)
				{
					return;
				}
				Face(Context, *State, AngleToward(Context, *State, *Target));
			}
			Advance(Context, 3);
		}

		/** FE 38: the ground distance between two actors, 0 when either is an empty party slot. */
		void GetActorDistance(FEventContext& Context)
		{
			int32 Distance = 0;
			const uint32 First = ReadActor(Context, 3);
			const uint32 Second = ReadActor(Context, 4);
			if (First != FEventContext::NoActor && Second != FEventContext::NoActor)
			{
				const FEventActorState* A = StateOf(Context, First);
				const FEventActorState* B = StateOf(Context, Second);
				if (A == nullptr || B == nullptr
					|| !Length(Context, (A->Position[0] >> 16) - (B->Position[0] >> 16), (A->Position[2] >> 16) - (B->Position[2] >> 16), 0, Distance))
				{
					return;
				}
			}
			SetVar(Context, ReadU16(Context, 1), Distance);
			Advance(Context, 5);
		}

		void GetFacingAngle(FEventContext& Context)
		{
			SetVar(Context, ReadU16(Context, 1), Self(Context).Facing & 0xFFF);
			Advance(Context, 3);
		}

		void GetActorFacing(FEventContext& Context)
		{
			const uint32 Actor = ReadActor(Context, 1);
			if (Actor != FEventContext::NoActor)
			{
				const FEventActorState* Target = StateOf(Context, Actor);
				if (Target == nullptr)
				{
					return;
				}
				SetVar(Context, ReadU16(Context, 2), Target->Facing & 0xFFF);
			}
			Advance(Context, 4);
		}
	}

	std::optional<uint16> DirectionAngle(FEventContext& Context, int32 Index)
	{
		if (Index < 0 || Index > 15)
		{
			Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Index));
			return std::nullopt;
		}
		return static_cast<uint16>(((Index - 2) & 7) << 9 | AngleSet);
	}

	void RegisterFacing(FBaseTable& Base, FExtendedTable& Extended)
	{
		Base[0x2E] = &GetFacing;
		Base[0x5F] = &SetFacingCode;
		Base[0x67] = &TurnActor<false>;
		Base[0x68] = &TurnActor<true>;
		Base[0x69] = &Turn;
		Base[0x6A] = &TurnOnScreen;
		Base[0x6B] = &TurnBy<1>;
		Base[0x6C] = &TurnBy<-1>;
		Base[0x6F] = &FaceActor;
		Base[0x70] = &FacePartyMember;
		Base[0x89] = &IfActorWithinDistance;
		Base[0xAA] = &FaceOnScreen;
		Extended[0x01] = &RandomTurn;
		Extended[0x17] = &TurnToActor;
		Extended[0x38] = &GetActorDistance;
		Extended[0x71] = &GetFacingAngle;
		Extended[0x75] = &GetActorFacing;
	}
}
