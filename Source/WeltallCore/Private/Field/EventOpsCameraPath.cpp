#include "EventOps.h"

#include "Field/FieldCamera.h"
#include "Graphics/GteVectors.h"

namespace Weltall::EventOps
{
	namespace
	{
		constexpr uint8 PathMoveMask = 0xF;
		constexpr uint8 SnapToStart = 0x80;

		/** A point the scripts give in whole units, x, z then y, each a constant or a variable by the flags byte. */
		FFieldPoint ReadPoint(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 7);
			FFieldPoint Point;
			Point[0] = static_cast<int32>(static_cast<uint32>(ReadOperand(Context, 1, Flags, 0x80)) << 16);
			Point[2] = static_cast<int32>(static_cast<uint32>(ReadOperand(Context, 3, Flags, 0x40)) << 16);
			Point[1] = static_cast<int32>(static_cast<uint32>(ReadOperand(Context, 5, Flags, 0x20)) << 16);
			return Point;
		}

		/** An actor's position, or the first party member's when the scripts name no actor; null when there is neither. */
		const FFieldPoint* ActorOrLeaderPosition(FEventContext& Context)
		{
			uint32 Actor = ReadActor(Context, 1);
			if (Actor == FEventContext::NoActor)
			{
				Actor = Context.PartyActors[0];
			}
			const FEventActorState* State = StateOf(Context, Actor);
			return State != nullptr ? &State->Position : nullptr;
		}

		/** 60 and 64: the path's target or eye starts where the follow puts it now. */
		template <FFieldPoint FFieldCamera::*From, FFieldPoint FFieldCamera::*Goal>
		void PathFromGoal(FEventContext& Context)
		{
			Context.Camera.*From = Context.Camera.*Goal;
			Context.Run.Budget += 1;
			Advance(Context, 1);
		}

		/** 61, 63, 65 and A3: a point of the path given in whole units. */
		template <FFieldPoint FFieldCamera::*Point>
		void PathPoint(FEventContext& Context)
		{
			Context.Camera.*Point = ReadPoint(Context);
			Context.Run.Budget += 1;
			Advance(Context, 8);
		}

		/** 62 and 66: the path's target or eye ends at an actor. */
		template <FFieldPoint FFieldCamera::*Point>
		void PathToActor(FEventContext& Context)
		{
			const FFieldPoint* Position = ActorOrLeaderPosition(Context);
			if (Position == nullptr)
			{
				return;
			}
			Context.Camera.*Point = *Position;
			Context.Run.Budget += 1;
			Advance(Context, 2);
		}

		/** AB: the path starts and ends where the follow puts the target and the eye now. */
		void PathFromCurrent(FEventContext& Context)
		{
			FFieldCamera& Camera = Context.Camera;
			Camera.TargetFrom = Camera.TargetGoal;
			Camera.TargetTo = Camera.TargetGoal;
			Camera.EyeFrom = Camera.EyeGoal;
			Camera.EyeTo = Camera.EyeGoal;
			Context.Run.Budget += 1;
			Advance(Context, 1);
		}

		/** The path's target or eye: its ends, its move, the point a snap puts at its start and its follow divisor. */
		struct FPathLeg
		{
			FFieldPoint FFieldCamera::*From;
			FFieldPoint FFieldCamera::*To;
			FFieldCameraMove FFieldCamera::*Move;
			FFieldPoint FFieldCamera::*Actual;
			int32 FFieldCamera::*Divisor;
			uint16 Bit;
		};

		constexpr FPathLeg TargetLeg = {&FFieldCamera::TargetFrom, &FFieldCamera::TargetTo, &FFieldCamera::TargetMove, &FFieldCamera::Target,
			&FFieldCamera::TargetDivisor, FieldCameraPathMoves::Target};
		constexpr FPathLeg EyeLeg = {&FFieldCamera::EyeFrom, &FFieldCamera::EyeTo, &FFieldCamera::EyeMove, &FFieldCamera::Eye,
			&FFieldCamera::EyeDivisor, FieldCameraPathMoves::Eye};

		/** Along the path over a number of ticks; none takes one, and the follow snaps. */
		void StartTimedMove(FEventContext& Context, const FPathLeg& Leg)
		{
			FFieldCamera& Camera = Context.Camera;
			FFieldCameraMove& Move = Camera.*Leg.Move;
			const int32 Ticks = ReadValue(Context, 2);
			Move.Ticks = static_cast<int16>(Ticks);
			if ((Ticks & 0xFFFF) == 0)
			{
				Move.Ticks = 1;
				Camera.*Leg.Divisor = 1;
			}
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				const uint32 Distance = static_cast<uint32>((Camera.*Leg.To)[Axis]) - static_cast<uint32>((Camera.*Leg.From)[Axis]);
				Move.Step[Axis] = ProcessorDivide(static_cast<int32>(Distance), Move.Ticks);
			}
			Move.At = Camera.*Leg.From;
			Camera.PathMoves |= Leg.Bit;
		}

		/** Along the path at a speed in whole units per tick, for as many ticks as its length takes. */
		bool StartPacedMove(FEventContext& Context, const FPathLeg& Leg)
		{
			FFieldCamera& Camera = Context.Camera;
			FGteLongVector Back;
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				const uint32 Difference = static_cast<uint32>((Camera.*Leg.From)[Axis]) - static_cast<uint32>((Camera.*Leg.To)[Axis]);
				Back[Axis] = static_cast<int32>(Difference) >> 16;
			}
			int32 PathLength = 0;
			if (!Length(Context, Back[0], Back[1], Back[2], PathLength))
			{
				return false;
			}
			const FGteLongVector Direction = VectorNormal(Back);
			const int32 Speed = ReadValue(Context, 2);
			FFieldCameraMove& Move = Camera.*Leg.Move;
			Move.At = Camera.*Leg.From;
			Camera.PathMoves |= Leg.Bit;
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Move.Step[Axis] = static_cast<int32>(static_cast<uint32>(Direction[Axis]) * static_cast<uint32>(Speed) * 0xFFFFFFF0u);
			}
			Move.Ticks = static_cast<int16>(ProcessorDivide(PathLength, Speed));
			return true;
		}

		/**
		 * AC: starts moving the target (0, 2) or the eye (1, 3) along the path, timed (0, 1) or at a
		 * speed (2, 3); bit 7 puts the camera at the path's start at once. The moves run in script mode.
		 */
		void PathMove(FEventContext& Context)
		{
			const uint8 Operation = CodeByte(Context, 1);
			const uint8 Kind = Operation & PathMoveMask;
			if (Kind < 4)
			{
				const FPathLeg& Leg = Kind % 2 == 0 ? TargetLeg : EyeLeg;
				if (Kind < 2)
				{
					StartTimedMove(Context, Leg);
				}
				else if (!StartPacedMove(Context, Leg))
				{
					return;
				}
				if ((Operation & SnapToStart) != 0)
				{
					Context.Camera.*Leg.Actual = Context.Camera.*Leg.From;
				}
			}
			Advance(Context, 4);
		}

		/** EF: yields a tick at a time while a move the mask names is under way (bit 0 the target's, bit 1 the eye's). */
		void WaitPathMoves(FEventContext& Context)
		{
			const int32 Mask = ReadValue(Context, 1);
			const FFieldCamera& Camera = Context.Camera;
			const int32 Moving = (Camera.TargetMove.Ticks != 0 ? FieldCameraPathMoves::Target : 0) | (Camera.EyeMove.Ticks != 0 ? FieldCameraPathMoves::Eye : 0);
			Context.Run.bYield = true;
			if ((Moving & Mask) == 0)
			{
				Advance(Context, 3);
			}
		}
	}

	void RegisterCameraPath(FBaseTable& Base, FExtendedTable&)
	{
		Base[0xEF] = &WaitPathMoves;
		Base[0x60] = &PathFromGoal<&FFieldCamera::TargetFrom, &FFieldCamera::TargetGoal>;
		Base[0x61] = &PathPoint<&FFieldCamera::TargetFrom>;
		Base[0x62] = &PathToActor<&FFieldCamera::TargetTo>;
		Base[0x63] = &PathPoint<&FFieldCamera::TargetTo>;
		Base[0x64] = &PathFromGoal<&FFieldCamera::EyeFrom, &FFieldCamera::EyeGoal>;
		Base[0x65] = &PathPoint<&FFieldCamera::EyeFrom>;
		Base[0x66] = &PathToActor<&FFieldCamera::EyeTo>;
		Base[0xA3] = &PathPoint<&FFieldCamera::EyeTo>;
		Base[0xAB] = &PathFromCurrent;
		Base[0xAC] = &PathMove;
	}
}
