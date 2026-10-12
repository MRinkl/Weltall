#include "EventMoves.h"
#include "EventSprites.h"
#include "MovementRules.h"

#include "Field/FieldArchive.h"
#include "Field/FieldFollowers.h"
#include "Graphics/Sprite.h"

// The party walking to its places as a group, the actors' movement doing the walking, and the change of
// the actor the player controls.
namespace Weltall::EventOps
{
	namespace
	{
		using namespace Moves;
		using MovementRules::WholePart;

		constexpr int32 PartySlots = 3;
		constexpr int32 WalkPace = 0x4000000;
		/** A member that has not moved for this many ticks is put on its place. */
		constexpr int16 StuckTicks = 0x41;
		/** An x of this names no places: the party only stops where it stands, facing as it does. */
		constexpr int32 StopWhereStanding = 0x7FFF;
		/** The direction operand that keeps the member's facing. */
		constexpr int32 KeepFacing = 0xFF;
		constexpr uint32 ClearedOnArrival = EventActorFlags0::AnimationLocked | EventActorFlags0::WalksToLeader | EventActorFlags0::Airborne;

		enum class EStep
		{
			Walking,
			Arrived,
			Fault,
		};

		/**
		 * One tick of a party member's walk to (X, Z): turned toward it while the actors' movement walks it
		 * at its sprite's speed, or, within a stride, stuck too long or hurried by FE B5, put there facing
		 * Direction (a direction 0-15, or KeepFacing; the locked facing while it is locked). An empty slot
		 * or a hidden actor has arrived at once.
		 */
		EStep StepMember(FEventContext& Context, int32 Slot, int32 X, int32 Z, int32 Direction)
		{
			const uint8 Actor = Context.PartyActors[static_cast<std::size_t>(Slot)];
			if (Actor == FEventContext::NoActor)
			{
				return EStep::Arrived;
			}
			const FFieldActor* Record = RecordOf(Context, Actor);
			if (Record == nullptr)
			{
				return EStep::Fault;
			}
			if ((Record->Flags & FieldActorFlag::Hidden) != 0)
			{
				return EStep::Arrived;
			}
			FSprite* Sprite = ActorSprite(Context, Actor);
			FEventActorState* State = StateOf(Context, Actor);
			if (Sprite == nullptr || State == nullptr)
			{
				Fail(Context, State == nullptr ? EEventFault::ActorWithoutScripts : EEventFault::ActorWithoutSprite, Actor);
				return EStep::Fault;
			}
			if (Sprite->Speed == 0)
			{
				Sprite->Speed = ProcessorDivide(WalkPace, State->SpeedDivisor);
			}
			const int32 Stride = RootOfSquare(Sprite->Speed >> 15);
			const int32 DeltaX = X - WholePart(State->Position[0]);
			const int32 DeltaZ = Z - WholePart(State->Position[2]);
			int32 Distance = 0;
			if (!Length(Context, DeltaX, DeltaZ, 0, Distance))
			{
				return EStep::Fault;
			}
			State->Flags0 |= EventActorFlags0::FollowsFloor;
			if (Stride + 1 < Distance)
			{
				const bool bStill = State->PreviousPosition[0] == static_cast<int16>(WholePart(State->Position[0]))
					&& State->PreviousPosition[1] == static_cast<int16>(WholePart(State->Position[1]))
					&& State->PreviousPosition[2] == static_cast<int16>(WholePart(State->Position[2]));
				State->StillTicks = bStill ? static_cast<int16>(State->StillTicks + 1) : int16{0};
				const uint16 Heading = DirectionOf(Context, DeltaX, DeltaZ);
				State->MoveDirection = Heading;
				State->Facing = Heading;
				if (State->StillTicks < StuckTicks && Context.PartyWalkHurries == 0)
				{
					return EStep::Walking;
				}
				if (!PlaceActorOnFloor(Context, Actor, X, Z))
				{
					return EStep::Fault;
				}
			}
			uint16 Facing = 0;
			if ((State->Flags0 & EventActorFlags0::FacingLocked) != 0)
			{
				Facing = State->LockedFacing | DirectionApplied;
			}
			else if (Direction == KeepFacing)
			{
				Facing = State->Facing | DirectionApplied;
			}
			else if (const std::optional<uint16> Angle = DirectionAngle(Context, Direction))
			{
				Facing = *Angle;
			}
			else
			{
				return EStep::Fault;
			}
			State->MoveDirection = Facing;
			State->Facing = Facing;
			State->Position[0] = static_cast<int32>(static_cast<uint32>(X) << 16);
			State->Position[2] = static_cast<int32>(static_cast<uint32>(Z) << 16);
			State->StillTicks = 0;
			State->Flags0 &= ~ClearedOnArrival;
			return EStep::Arrived;
		}

		/** The members' steps of one tick, as a mask of the slots that have arrived; false on a fault. */
		struct FGroupStep
		{
			uint32 Arrived = 0;
			bool bFault = false;

			void Add(int32 Slot, EStep Step)
			{
				bFault = bFault || Step == EStep::Fault;
				Arrived |= Step == EStep::Arrived ? 1u << Slot : 0u;
			}

			bool AllArrived() const { return Arrived == (1u << PartySlots) - 1; }
		};

		/**
		 * FE 23: the party's members walk to their places as a group (each x and z by the flags at 13, the
		 * direction each faces there at 14, 16 and 18), the party's actors running no scripts meanwhile;
		 * the instruction waits until all have arrived. An x of 7FFFh only marks each member as standing
		 * where it is. Either way the members stay where the scripts put them rather than follow the leader.
		 */
		void PartyWalkTo(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 13);
			if (ReadOperand(Context, 1, Flags, 0x80) == StopWhereStanding)
			{
				Advance(Context, 20);
				Context.Controls.bPartyPlaced = true;
				for (const uint8 Actor : Context.PartyActors)
				{
					if (Actor == FEventContext::NoActor)
					{
						continue;
					}
					FEventActorState* State = StateOf(Context, Actor);
					if (State == nullptr)
					{
						return;
					}
					const uint16 Stopped = State->Facing | DirectionApplied;
					State->MoveDirection = Stopped;
					State->Facing = Stopped;
				}
				return;
			}
			FGroupStep Group;
			for (int32 Slot = 0; Slot < PartySlots && !Group.bFault; ++Slot)
			{
				const int32 X = ReadOperand(Context, 1 + 4 * Slot, Flags, static_cast<uint8>(0x80 >> (2 * Slot)));
				const int32 Z = ReadOperand(Context, 3 + 4 * Slot, Flags, static_cast<uint8>(0x40 >> (2 * Slot)));
				const int32 Direction = ReadValue(Context, 14 + 2 * Slot);
				Group.Add(Slot, StepMember(Context, Slot, X, Z, Direction));
			}
			if (Group.bFault)
			{
				return;
			}
			Context.Run.bYield = true;
			if (Group.AllArrived())
			{
				Context.PartyWalkHurries = 0;
				Advance(Context, 20);
				Context.bPartyWalking = false;
			}
			else
			{
				Context.bPartyWalking = true;
				Advance(Context, -1);
			}
			Context.Controls.bPartyPlaced = true;
		}

		/**
		 * FE 24: the party's members walk to where the leader stands, as FE 23 walks them; once all are
		 * there they follow the leader again, its trail started afresh.
		 */
		void PartyGather(FEventContext& Context)
		{
			const FEventActorState* Leader = StateOf(Context, Context.PartyActors[0]);
			if (Leader == nullptr)
			{
				return;
			}
			const int32 X = WholePart(Leader->Position[0]);
			const int32 Z = WholePart(Leader->Position[2]);
			FGroupStep Group;
			for (int32 Slot = 0; Slot < PartySlots && !Group.bFault; ++Slot)
			{
				Group.Add(Slot, StepMember(Context, Slot, X, Z, KeepFacing));
			}
			if (Group.bFault)
			{
				return;
			}
			Context.Run.bYield = true;
			if (!Group.AllArrived())
			{
				Context.bPartyWalking = true;
				Advance(Context, -1);
				return;
			}
			Advance(Context, 1);
			Context.bPartyWalking = false;
			Context.PartyWalkHurries = 0;
			Context.Controls.bPartyPlaced = false;
			if (!RestartFollowTrail(Context))
			{
				Fail(Context, EEventFault::NoActorModels, Context.LeaderActor);
			}
		}

		/** FE B5: the party's walk to its places ends at the next tick, every member still walking put on its place. */
		void HurryPartyWalk(FEventContext& Context)
		{
			Context.PartyWalkHurries = static_cast<int16>(Context.PartyWalkHurries + 1);
			Advance(Context, 1);
		}

		/**
		 * FE B6: the player controls another actor, which the camera follows; the others the pad
		 * controlled or had following lose it, and while the actor is not the party's first member the
		 * party's others only stand.
		 */
		void SetPlayerActor(FEventContext& Context)
		{
			const uint32 Actor = ReadActor(Context, 1);
			if (Actor != FEventContext::NoActor)
			{
				Context.Controls.bLeaderApart = Actor != Context.PartyActors[0];
				Context.Camera.FollowedActor = static_cast<uint8>(Actor);
				Context.LeaderActor = static_cast<uint8>(Actor);
				for (FEventActorState& State : Context.States)
				{
					State.Flags0 &= ~(EventActorFlags0::PadControlled | EventActorFlags0::PartyFollower);
				}
				FEventActorState* State = StateOf(Context, Actor);
				if (State == nullptr)
				{
					return;
				}
				State->Flags0 |= EventActorFlags0::PadControlled;
			}
			Advance(Context, 2);
		}
	}

	void RegisterPartyWalks(FBaseTable&, FExtendedTable& Extended)
	{
		Extended[0x23] = &PartyWalkTo;
		Extended[0x24] = &PartyGather;
		Extended[0xB5] = &HurryPartyWalk;
		Extended[0xB6] = &SetPlayerActor;
	}
}
