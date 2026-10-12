#include "Field/FieldWalkTrace.h"

#include "Field/FieldWalkmesh.h"
#include "MovementRules.h"

#include "Field/FieldCamera.h"
#include "Graphics/Gte.h"
#include "Graphics/GteVectors.h"

namespace Weltall
{
	namespace
	{
		using namespace MovementRules;

		/** The probes look 4 units past the move, 22.5 degrees to either side and straight on. */
		constexpr int32 ProbeReach = 0x40;
		constexpr int32 ProbeSpread = 0x100;
		/** A drop at least this deep is walked off, to fall. */
		constexpr int32 Ledge = 0x40;

		std::array<int32, 3> Probe(const std::array<int32, 3>& Delta, int32 Angle)
		{
			const int32 Masked = Angle & 0xFFF;
			return {Add(Delta[0], GteCos(Masked) * ProbeReach), 0, Add(Delta[2], GteSin(Masked) * -ProbeReach)};
		}

		/**
		 * The probes in their order, then the move they leave: the move itself, or the slide along the
		 * wall the first failing probe met.
		 */
		std::array<int32, 3> ProbedMove(const FEventContext& Context, const FEventActorState& State, const std::array<int32, 3>& Delta, int32 Direction,
			const std::array<int32, 3>& Angles, EWalkRule Rule, FWalkTrace& Trace)
		{
			for (const int32 Angle : Angles)
			{
				if (!TraceWalk(Context, State, Probe(Delta, Angle), Rule, true, Trace))
				{
					std::array<int32, 3> Slid = Delta;
					SlideAlongEdge(Context, Direction, Trace.Edge, Slid);
					return Slid;
				}
			}
			return Delta;
		}

		/** The move made: its horizontal part, and the drop from the feet to the floor reached. */
		void Arrive(FEventActorState& State, std::array<int32, 3>& Delta, const std::array<int32, 3>& Move, int16 Floor)
		{
			Delta = {Move[0], Wrap(int64{Floor} * 0x10000 - State.Position[1]), Move[2]};
			State.FloorHeight = static_cast<int16>(WholePart(Add(State.Position[1], Delta[1])));
		}
	}

	bool MoveOnGround(const FEventContext& Context, FEventActorState& State, std::array<int32, 3>& Delta, int32 Direction, FWalkEdge& Edge)
	{
		const int16 Heading = static_cast<int16>(Direction);
		FWalkTrace Trace;
		Trace.Edge = Edge;
		std::array<int32, 3> Move = ProbedMove(Context, State, Delta, Heading, {Heading - ProbeSpread, Heading + ProbeSpread, Direction}, EWalkRule::Ground, Trace);
		bool bMoved = TraceWalk(Context, State, Move, EWalkRule::Ground, false, Trace);
		if (bMoved)
		{
			const int32 Feet = WholePart(State.Position[1]);
			const int32 Floor = Trace.Point[1];
			bool bTakenAsItIs = false;
			if (Feet <= Floor && (Trace.Attributes & FieldFloorAttribute::Ladder) == 0)
			{
				bTakenAsItIs = (Trace.Attributes & FieldFloorAttribute::Slopes) == 0 ? Feet + Ledge <= Floor : (State.FloorAttributes & FieldFloorAttribute::Slopes) == 0;
			}
			if (!bTakenAsItIs)
			{
				// Along the floor's rise: the move's horizontal length, in the direction of the step to the new floor.
				const FGteLongVector Rise{Wrap(-int64{Move[0]}) >> 8, Wrap(int64{Floor} * 0x10000 - State.Position[1]) >> 8, Wrap(-int64{Move[2]}) >> 8};
				const FGteLongVector Unit = VectorNormal(Rise);
				const int32 Length = FieldLength2D(Move[0] >> 8, Move[2] >> 8);
				Move = {Wrap(-int64{Multiply(Length, Unit[0])}) >> 4, Multiply(Length, Unit[1]) >> 4, Wrap(-int64{Multiply(Length, Unit[2])}) >> 4};
				bMoved = TraceWalk(Context, State, Move, EWalkRule::Ground, false, Trace);
				if (bMoved)
				{
					State.Flags0 |= EventActorFlags0::MovedAlongSlope;
				}
			}
		}
		if (bMoved)
		{
			Arrive(State, Delta, Move, Trace.Point[1]);
		}
		Edge = Trace.Edge;
		return bMoved;
	}

	bool MoveThroughAir(const FEventContext& Context, FEventActorState& State, std::array<int32, 3>& Delta, int32 Direction, FWalkEdge& Edge)
	{
		const int16 Heading = static_cast<int16>(Direction);
		FWalkTrace Trace;
		Trace.Edge = Edge;
		const std::array<int32, 3> Move = ProbedMove(Context, State, Delta, Heading, {Direction, Heading - ProbeSpread, Heading + ProbeSpread}, EWalkRule::Air, Trace);
		bool bMoved = TraceWalk(Context, State, Move, EWalkRule::Air, false, Trace);
		int16 Floor = Trace.Point[1];
		if (bMoved)
		{
			if ((State.Flags0 & EventActorFlags0::FixedHeight) != 0)
			{
				Floor = State.TargetHeight;
			}
			else
			{
				bMoved = int32{Floor} * 0x10000 >= State.Position[1];
			}
		}
		if (bMoved)
		{
			Arrive(State, Delta, Move, Floor);
		}
		Edge = Trace.Edge;
		return bMoved;
	}
}
