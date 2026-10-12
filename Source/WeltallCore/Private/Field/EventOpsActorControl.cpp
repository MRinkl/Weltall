#include "EventOps.h"
#include "EventSprites.h"

#include "Game/PsxRandom.h"
#include "Graphics/Sprite.h"

namespace Weltall::EventOps
{
	namespace
	{
		constexpr uint16 Hidden = 0x20;
		constexpr uint16 DirectionApplied = 0x8000;
		constexpr int32 AngleMask = 0xFFF;
		/** An eighth of a turn. */
		constexpr int32 WanderTurn = 0x200;

		/** 21: how slowly the actor moves and animates: scripted moves go 0x4000000 / Divisor per tick. */
		void SetSpeed(FEventContext& Context)
		{
			const int32 Divisor = ReadValue(Context, 1);
			Self(Context).SpeedDivisor = static_cast<uint16>(Divisor);
			if (FSprite* Sprite = ActorSprite(Context, Context.Run.Actor))
			{
				Sprite->SetTimeScale(Divisor & 0xFFFF);
			}
			Advance(Context, 3);
		}

		/**
		 * F6: 1 locks the actor's facing while it moves, 0 unlocks it and stops a sprite that moved by
		 * its own velocity (not its fall), 2 has its movement drive its sprite's velocity.
		 */
		void SetMoveMode(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			switch (CodeByte(Context, 1))
			{
				case 0:
					State.Flags0 &= ~EventActorFlags0::FacingLocked;
					if ((State.Flags1 & EventActorFlags1::MovesBySpriteVelocity) != 0)
					{
						if (FSprite* Sprite = ActorSprite(Context, Context.Run.Actor))
						{
							Sprite->Velocity[0] = 0;
							Sprite->Velocity[2] = 0;
							Sprite->Speed = 0;
						}
						State.Flags1 &= ~EventActorFlags1::MovesBySpriteVelocity;
					}
					break;
				case 1:
					State.Flags0 |= EventActorFlags0::FacingLocked;
					State.LockedFacing = State.Facing;
					break;
				case 2:
					State.Flags1 |= EventActorFlags1::MovesBySpriteVelocity;
					break;
				default:
					break;
			}
			Advance(Context, 2);
		}

		/**
		 * FE 13 and FE 14: the sound the actor gives off where it stands (0 none) and its volume, in a
		 * mode the two differ by and nothing reads. The channel the actor's sound held stops and is free
		 * again, for the field's positional sounds to give out anew.
		 */
		template <uint8 Mode>
		void SetActorSound(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			State.Sound = static_cast<uint16>(ReadValue(Context, 1));
			State.SoundMode = Mode;
			State.SoundVolume = static_cast<uint8>(ReadValue(Context, 3));
			Advance(Context, 5);
			ReleaseActorSound(Context, Context.Run.Actor);
			if (State.Sound == 0)
			{
				State.SoundMode = FFieldActorSounds::Silent;
			}
		}

		/**
		 * 5B: the running actor stands still: its steps and velocity cleared, its move direction and
		 * facing marked applied, its sprite's own motion stopped. It yields on the instruction without
		 * moving on, so the thread stands still every tick until something moves its pc.
		 */
		void StandStill(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			Context.Run.bYield = true;
			State.Step = {};
			State.Velocity = {};
			const uint16 Direction = static_cast<uint16>(State.MoveDirection | DirectionApplied);
			State.Facing = Direction;
			State.MoveDirection = Direction;
			if (FSprite* Sprite = ActorSprite(Context, Context.Run.Actor))
			{
				Sprite->Velocity[0] = 0;
				Sprite->Velocity[2] = 0;
				Sprite->Speed = 0;
			}
		}

		/**
		 * 59: a tick of wandering. On every 16th tick of the actor's counter a rand() turns its walk
		 * 45 degrees left or right of its facing (one draw in four) or stops it, marking its facing
		 * applied; the other ticks it walks on as it faces. It yields.
		 */
		void WanderStep(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			uint16 Direction = State.Facing;
			State.Counter = static_cast<int16>(State.Counter + 1);
			if ((State.Counter & 0xF) == 0)
			{
				const int32 Draw = Context.Random->Next();
				if ((Draw & 0x30) == 0)
				{
					Direction = static_cast<uint16>(((Draw & 1) == 0 ? State.Facing + WanderTurn : State.Facing - WanderTurn) & AngleMask);
				}
				else
				{
					Direction = static_cast<uint16>(State.Facing | DirectionApplied);
					State.Facing = Direction;
				}
			}
			Context.Run.bYield = true;
			State.MoveDirection = Direction;
			Advance(Context, 1);
		}

		/** 5A: 5B's stop, once: the next tick goes on after it. */
		void Stop(FEventContext& Context)
		{
			StandStill(Context);
			Advance(Context, 1);
		}

		/** 27: stops an actor where it is: its scripts, motion and turning; the running actor's own window is asked to close. */
		void FreezeActor(FEventContext& Context)
		{
			const uint32 Actor = ReadActor(Context, 1);
			if (Actor != FEventContext::NoActor)
			{
				FEventActorState* Target = StateOf(Context, Actor);
				if (Target == nullptr)
				{
					return;
				}
				Target->Step = {};
				Target->Velocity = {};
				Target->Flags0 |= EventActorFlags0::Frozen;
				const uint16 Direction = static_cast<uint16>(Target->MoveDirection | DirectionApplied);
				Target->Facing = Direction;
				Target->MoveDirection = Direction;
				Context.Messages.RequestClose(Context.Run.Actor);
			}
			Advance(Context, 2);
		}

		/**
		 * 29: takes an actor out of the map for good: frozen, hidden, its scripts never run or start
		 * again; the running actor's own window is asked to close.
		 */
		void RemoveActor(FEventContext& Context)
		{
			const uint32 Actor = ReadActor(Context, 1);
			if (Actor != FEventContext::NoActor)
			{
				FEventActorState* Target = StateOf(Context, Actor);
				if (Target == nullptr || Actor >= Context.Actors.size())
				{
					return;
				}
				Target->Flags0 |= EventActorFlags0::Frozen;
				Target->Flags1 |= EventActorFlags1::Removed;
				Context.Actors[Actor].Flags |= Hidden;
				Context.Messages.RequestClose(Context.Run.Actor);
			}
			Advance(Context, 2);
		}
	}

	void RegisterActorControl(FBaseTable& Base, FExtendedTable& Extended)
	{
		Base[0xF6] = &SetMoveMode;
		Extended[0x13] = &SetActorSound<0>;
		Extended[0x14] = &SetActorSound<0x80>;
		Base[0x21] = &SetSpeed;
		Base[0x27] = &FreezeActor;
		Base[0x29] = &RemoveActor;
		Base[0x59] = &WanderStep;
		Base[0x5A] = &Stop;
		Base[0x5B] = &StandStill;
	}
}
