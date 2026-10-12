#include "EventMoves.h"
#include "EventSprites.h"
#include "FieldDirections.h"
#include "MovementRules.h"

#include "Field/FieldActorMotion.h"
#include "Graphics/Gte.h"
#include "Graphics/Sprite.h"

namespace Weltall::EventOps
{
	namespace
	{
		using MovementRules::WholePart;

		constexpr uint32 EntranceVariable = 0x002;
		constexpr uint32 LeftMapVariable = 0x004;
		constexpr uint32 LeaderDirectionVariable = 0x006;
		constexpr uint32 CameraDirectionVariable = 0x008;
		constexpr uint32 MapChangesVariable = 0x012;
		constexpr int32 MapNumberBits = 0x3FFF;
		constexpr uint16 PlayerLocked = 0xFFFF;
		/** The leader walks to a door at speed 8, to a point 40 units before it, ignoring every layer's floor attributes. */
		constexpr int32 DoorWalkSpeed = 0x80000;
		constexpr int32 DoorApproach = 0x28;
		constexpr uint32 DoorWalkAttributesIgnored = 0x7u << EventActorFlags1::AttributesIgnoredShift;
		/** Within this of the point (twice the walk's speed of 8), or stuck on the spot for this many ticks, the leader has arrived. */
		constexpr int32 DoorReach = 16;
		constexpr int16 DoorStuckTicks = 0x41;
		/** 47 heads for a point turned 20h short of the door's facing; EA straight in front of it. */
		constexpr int32 EnterDoorAngle = 0x3E0;
		constexpr int32 ExitDoorAngle = 0;
		constexpr int32 QuarterTurn = 0x400;

		/**
		 * The first request of the tick records where the party leaves from and which map and
		 * entrance it goes to (the operands at MapOffset and EntranceOffset); a later one changes nothing.
		 */
		void RecordMapChange(FEventContext& Context, int32 MapOffset, int32 EntranceOffset)
		{
			if (!Context.Requests.bMapChange)
			{
				const int32 Entrance = ReadValue(Context, EntranceOffset);
				const int32 Map = ReadValue(Context, MapOffset);
				SaveMapExit(Context);
				Context.Requests.bMapChange = true;
				SetVar(Context, EntranceVariable, Entrance);
				Context.MapNumber = Map;
			}
		}

		/**
		 * 98's work, which 12 runs too, reading the map at operand 1 and the entrance at 3: once no movie
		 * is to play, the player loses the party and the change is recorded. The actor's turn ends;
		 * false when it waits, the pc left on the instruction.
		 */
		bool RequestMapChange(FEventContext& Context)
		{
			Context.Run.bYield = true;
			if (Context.Requests.bMovie)
			{
				return false;
			}
			Context.PlayerLock = PlayerLocked;
			RecordMapChange(Context, 1, 3);
			Advance(Context, 5);
			return true;
		}

		/** 98: asks for another map and one of its entrances, once the field is free for it. */
		void ChangeMap(FEventContext& Context)
		{
			if (IsFieldBusy(Context))
			{
				Context.Run.bYield = true;
				return;
			}
			RequestMapChange(Context);
		}

		/**
		 * 12: 98 with the screen's transition and its ticks for the change, which runs once the scripts
		 * end. The transition's operands follow where 98's part leaves the pc: past the map and the
		 * entrance, or, while a movie is to play, on the instruction itself, whose first bytes are then
		 * taken for the transition and which the pc then stops inside (unless the fix waits whole).
		 */
		void ChangeMapWithTransition(FEventContext& Context)
		{
			if (IsFieldBusy(Context) || (Context.Options.bChangeMapWaitsWhole && Context.Requests.bMovie))
			{
				Context.Run.bYield = true;
				return;
			}
			RequestMapChange(Context);
			Context.Requests.MapTransition = ReadValue(Context, 0);
			Context.Requests.MapTransitionTicks = ReadValue(Context, 2);
			Advance(Context, 4);
		}
	}

	namespace
	{
		/** A door's walk is over once the map changes; FE 68's instruction waits until the leader stops. */
		enum class ELeaderWalk
		{
			ThroughDoor,
			ToPoint,
		};

		/**
		 * The walk's first steps each tick: the player loses the party, and the leader ignores every
		 * layer's floor attributes and walks at speed 8. Null, with a fault, when the leader has no sprite.
		 */
		FSprite* BeginLeaderWalk(FEventContext& Context, FEventActorState& Leader)
		{
			Context.PlayerLock = PlayerLocked;
			Leader.Flags1 |= DoorWalkAttributesIgnored;
			FSprite* Sprite = ActorSprite(Context, Context.LeaderActor);
			if (Sprite == nullptr)
			{
				Fail(Context, EEventFault::ActorWithoutSprite, static_cast<uint32>(Context.LeaderActor));
				return nullptr;
			}
			Sprite->Speed = DoorWalkSpeed;
			return Sprite;
		}

		/**
		 * The leader stops: facing where it walked, idle, its thread's move reset, the attributes seen
		 * again after FE 68's walk; the running actor moves past its instruction.
		 */
		void StopLeaderWalk(FEventContext& Context, FEventActorState& Leader, FSprite& Sprite, ELeaderWalk Kind)
		{
			const uint16 Stopped = static_cast<uint16>(Leader.Facing | Moves::DirectionApplied);
			Leader.MoveDirection = Stopped;
			Leader.Facing = Stopped;
			Sprite.Speed = 0;
			Leader.Animation = 0;
			PlayActorAnimation(Context, Context.LeaderActor, 0);
			Context.Run.bYield = true;
			FEventThread& Thread = Leader.Running();
			Moves::SetCountdown(Thread, Moves::NoCountdown);
			Thread.Word &= ~FEventThread::MoveStageMask;
			Leader.Flags0 &= ~EventActorFlags0::WalksToLeader;
			if (Kind == ELeaderWalk::ToPoint)
			{
				Leader.Flags1 &= ~DoorWalkAttributesIgnored;
			}
			Leader.StillTicks = 0;
			Advance(Context, 6);
		}

		/**
		 * One tick of the leader's walk from (FromX, FromZ), where it stood at the tick's start, to (X, Z):
		 * turned toward the point while the actors' movement walks it, or stopped once within 16 of it or
		 * stuck on the spot for 41h ticks. The actor's turn ends either way; FE 68's instruction runs again
		 * while the leader walks. True once the leader has stopped; nothing on a fault.
		 */
		std::optional<bool> StepLeaderWalk(FEventContext& Context, FEventActorState& Leader, FSprite& Sprite, int32 FromX, int32 FromZ, int32 X, int32 Z, ELeaderWalk Kind)
		{
			int32 Distance = 0;
			if (!Length(Context, X - FromX, 0, Z - FromZ, Distance))
			{
				return std::nullopt;
			}
			if (Distance > DoorReach)
			{
				const bool bStill = Leader.PreviousPosition[0] == static_cast<int16>(WholePart(Leader.Position[0]))
					&& Leader.PreviousPosition[1] == static_cast<int16>(WholePart(Leader.Position[1]))
					&& Leader.PreviousPosition[2] == static_cast<int16>(WholePart(Leader.Position[2]));
				Leader.StillTicks = bStill ? static_cast<int16>(Leader.StillTicks + 1) : int16{0};
				if (Leader.StillTicks < DoorStuckTicks)
				{
					const uint16 Heading = Moves::DirectionOf(Context, X - FromX, Z - FromZ);
					Leader.MoveDirection = Heading;
					Leader.Facing = Heading;
					Context.Run.bYield = true;
					if (Kind == ELeaderWalk::ToPoint)
					{
						Advance(Context, -1);
					}
					return false;
				}
			}
			StopLeaderWalk(Context, Leader, Sprite, Kind);
			return true;
		}

		/**
		 * 47 and EA: the leader walks through the running actor's door to another map (operand 2) and
		 * entrance (4), once the field is free for it. The player loses the party, the leader passes
		 * through actors, and the first request records the change, which stops the scripts; the leader
		 * heads for a point 40 units before the door (turned by AngleOffset from its facing).
		 */
		template <int32 AngleOffset>
		void WalkThroughDoor(FEventContext& Context)
		{
			if (IsFieldBusy(Context))
			{
				Context.Run.bYield = true;
				return;
			}
			FEventActorState* Leader = StateOf(Context, Context.LeaderActor);
			if (Leader == nullptr)
			{
				return;
			}
			Leader->Flags0 |= EventActorFlags0::PassesThroughActors;
			FSprite* Sprite = BeginLeaderWalk(Context, *Leader);
			if (Sprite == nullptr)
			{
				return;
			}
			const int32 LeaderX = WholePart(Leader->Position[0]);
			const int32 LeaderZ = WholePart(Leader->Position[2]);
			RecordMapChange(Context, 2, 4);
			const FEventActorState& Door = Self(Context);
			const int32 Angle = Context.Actors[static_cast<std::size_t>(Context.Run.Actor)].Rotation[1] + AngleOffset - QuarterTurn;
			const int32 X = Door.InteractX + WholePart(Door.Position[0]) + (GteCos(Angle) * DoorApproach >> 12);
			const int32 Z = Door.InteractZ + WholePart(Door.Position[2]) + (GteSin(Angle) * -DoorApproach >> 12);
			StepLeaderWalk(Context, *Leader, *Sprite, LeaderX, LeaderZ, X, Z, ELeaderWalk::ThroughDoor);
		}

		/**
		 * FE 68: the leader walks to a point (x and z by the flags at 5) as it walks through a door, once
		 * the field is free for it, and the instruction waits until it stops there. The leader passes
		 * through actors meanwhile, and after only if it did before the walk; the player does not get
		 * the party back.
		 */
		void WalkLeaderTo(FEventContext& Context)
		{
			if (IsFieldBusy(Context))
			{
				Context.Run.bYield = true;
				Advance(Context, -1);
				return;
			}
			const uint8 Flags = CodeByte(Context, 5);
			const int32 X = ReadOperand(Context, 1, Flags, 0x80);
			const int32 Z = ReadOperand(Context, 3, Flags, 0x40);
			FEventActorState* Leader = StateOf(Context, Context.LeaderActor);
			if (Leader == nullptr)
			{
				return;
			}
			if (Context.LeaderFlagsBeforeWalk == 0)
			{
				Context.LeaderFlagsBeforeWalk = Leader->Flags0;
			}
			Leader->Flags0 |= EventActorFlags0::PassesThroughActors;
			FSprite* Sprite = BeginLeaderWalk(Context, *Leader);
			if (Sprite == nullptr)
			{
				return;
			}
			const std::optional<bool> bStopped = StepLeaderWalk(Context, *Leader, *Sprite, WholePart(Leader->Position[0]), WholePart(Leader->Position[2]), X, Z, ELeaderWalk::ToPoint);
			if (bStopped.value_or(false))
			{
				if ((Context.LeaderFlagsBeforeWalk & EventActorFlags0::PassesThroughActors) == 0)
				{
					Leader->Flags0 &= ~EventActorFlags0::PassesThroughActors;
				}
				Context.LeaderFlagsBeforeWalk = 0;
			}
		}
	}

	void SaveMapExit(FEventContext& Context)
	{
		SetVar(Context, LeftMapVariable, Context.MapNumber & MapNumberBits);
		const FEventActorState* Leader = StateOf(Context, Context.LeaderActor);
		if (Leader == nullptr)
		{
			return;
		}
		SetVar(Context, LeaderDirectionVariable, FieldDirections::FacingEighth(Leader->Facing));
		SetVar(Context, CameraDirectionVariable, FieldDirections::CameraEighth(Context.Camera));
		SetVar(Context, MapChangesVariable, static_cast<int16>(GetVar(Context, MapChangesVariable) + 1));
	}

	void RegisterMapChanges(FBaseTable& Base, FExtendedTable& Extended)
	{
		Extended[0x68] = &WalkLeaderTo;
		Base[0x47] = &WalkThroughDoor<EnterDoorAngle>;
		Base[0xEA] = &WalkThroughDoor<ExitDoorAngle>;
		Base[0x12] = &ChangeMapWithTransition;
		Base[0x98] = &ChangeMap;
	}
}
