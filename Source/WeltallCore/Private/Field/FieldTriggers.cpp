#include "Field/FieldTriggers.h"

#include "Field/EventContext.h"
#include "Field/EventMachine.h"
#include "Field/FieldActorBox.h"
#include "Graphics/Gte.h"
#include "Graphics/GteMath.h"
#include "Graphics/Sprite.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr uint16 ActionButton = 0x20;
		/** First flag word: talking to the actor, or touching it, starts nothing. */
		constexpr uint32 TalkBlocked = 0x220000;
		constexpr uint32 TouchBlocked = 0xA20000;
		constexpr uint32 BoxShaped = 0x2000;
		/** A platform that stops the leader's fall when it touches it. */
		constexpr uint32 StopsFall = 0x8000000;
		/** Second flag word: a platform, and one the leader's collisions found it on this tick. */
		constexpr uint32 PlatformBits = 0x180;
		constexpr uint32 LeaderOnPlatform = 0x100;
		/** A box-shaped actor that talks only to a leader facing it. */
		constexpr uint32 TalksWhenFaced = 0x40000;
		constexpr int32 BoxMargin = 0x10;
		constexpr int32 TouchReach = 8;
		constexpr int32 TalkReach = 0x20;
		constexpr uint32 FacingShift = 9;
		constexpr uint32 FacingMask = 0x7u << FacingShift;
		constexpr uint16 TurnSet = 0x8000;
		constexpr uint32 AngleMask = 0xFFF;
		/** The leader faces the actor when the difference of directions is below 700 or from 3401 (4096 a turn). */
		constexpr uint32 FacedBelow = 700;
		constexpr uint32 UnfacedSpan = 0xA89;
		/** In an actor's direction: none is asked for. */
		constexpr uint16 NoDirection = 0x8000;
		/** The ticks the leader is held up at a door it walks at before the door opens by itself. */
		constexpr int16 DoorPushTicks = 2;
		/** How far a door's interaction point lies along its panel from its hinge: half the panel's width. */
		constexpr int32 DoorHalfWidth = 36;
		/**
		 * A walk is held up or turned aside when it leaves the way asked for by more than a quarter of what it makes
		 * along it, and comes no nearer a door when less than a quarter of its step is toward the door's panel.
		 */
		constexpr int64 HeldUpRatio = 4;

		int32 Whole(int32 Value)
		{
			return static_cast<int16>(Value >> 16);
		}

		struct FScan
		{
			FEventContext& Context;
			int32 X = 0;
			int32 Z = 0;
			int32 Feet = 0;
			int32 Head = 0;
			int32 TouchRadius = 0;
			int32 TalkRadius = 0;
			uint32 Facing = 0;
			/** The direction the leader is asked to walk in. */
			uint16 Asked = 0;
			/** Only one actor a tick may be talked to. */
			bool bTalkTaken = false;
			/** The door the leader's walking into it asks to talk to this tick, as the button would with the door faced. */
			const FEventActorState* Pushed = nullptr;
		};

		struct FChoice
		{
			int32 Script = -1;
			uint32 Priority = 0;
		};

		int32 AngleTo(const FScan& Scan, const FEventActorState& Actor)
		{
			const int32 Dx = Whole(Actor.Position[0]) - Scan.X + Actor.InteractX;
			const int32 Dz = Whole(Actor.Position[2]) - Scan.Z + Actor.InteractZ;
			return GteRatan2(Dz, Dx, *Scan.Context.Arctangents);
		}

		/** Turns the actor toward the leader; the talk rounds the direction down, the touch up. */
		void TurnForTalk(FEventActorState& Actor, int32 Angle)
		{
			Actor.Status = (Actor.Status & ~FacingMask) | (static_cast<uint32>(-Angle) & FacingMask);
		}

		void TurnForTouch(FEventActorState& Actor, int32 Angle)
		{
			Actor.Status = (Actor.Status & ~FacingMask) | (static_cast<uint32>(-(Angle >> FacingShift)) & 7) << FacingShift;
		}

		bool IsFaced(const FScan& Scan, int32 Angle)
		{
			const uint32 Difference = (Scan.Facing - (static_cast<uint32>(-Angle) & AngleMask)) & AngleMask;
			return Difference - FacedBelow >= UnfacedSpan;
		}

		/** Whether the leader faces the actor as a talk asks: a door walked into is faced by the walk at it. */
		bool IsFacedForTalk(const FScan& Scan, const FEventActorState& Actor, int32 Angle)
		{
			return &Actor == Scan.Pushed || IsFaced(Scan, Angle);
		}

		bool WantsToTalk(const FScan& Scan, const FEventActorState& Actor)
		{
			const bool bAsked = (Scan.Context.Controls.Pressed & ActionButton) != 0 || &Actor == Scan.Pushed;
			return bAsked && !Scan.bTalkTaken && (Actor.Flags1 & EventActorFlags1::NoTalkScript) == 0;
		}

		void ChooseTalk(FScan& Scan, FEventActorState& Actor, int32 Angle, FChoice& Choice)
		{
			if (&Actor == Scan.Pushed)
			{
				FFieldControls& Controls = Scan.Context.Controls;
				Controls.AnsweredDoor = Controls.PushedDoor;
				Controls.AnsweredDirection = Scan.Asked;
			}
			Scan.bTalkTaken = true;
			Choice = {FEventMachine::TalkScript, FEventMachine::TalkPriority};
			TurnForTalk(Actor, Angle);
		}

		void ChooseTouch(FEventActorState& Actor, int32 Angle, FChoice& Choice)
		{
			Choice = {FEventMachine::TouchScript, FEventMachine::TouchPriority};
			TurnForTouch(Actor, Angle);
		}

		/** A platform: talked to or touched when the leader is on it; another platform releases the latch. */
		void CheckPlatform(FScan& Scan, FEventActorState& Actor, FChoice& Choice)
		{
			FFieldControls& Controls = Scan.Context.Controls;
			if ((Actor.Flags1 & LeaderOnPlatform) == 0)
			{
				Controls.bTouchLatched = false;
			}
			else if (WantsToTalk(Scan, Actor))
			{
				if ((Actor.Flags0 & TalkBlocked) == 0 && Scan.Context.Messages.OpenMask == 0)
				{
					ChooseTalk(Scan, Actor, AngleTo(Scan, Actor), Choice);
				}
			}
			else if ((Actor.Flags0 & TouchBlocked) == 0)
			{
				ChooseTouch(Actor, AngleTo(Scan, Actor), Choice);
				if (!Controls.bTouchLatched && (Actor.Flags0 & StopsFall) != 0)
				{
					Controls.bTouchLatched = true;
					if (FSprite* Sprite = Scan.Context.Models != nullptr ? Scan.Context.Models->ActorSprite(Scan.Context.LeaderActor) : nullptr)
					{
						Sprite->Velocity[1] = 0;
					}
				}
			}
		}

		/** Whether the actor's interaction point is at the height of the leader's body. */
		bool OverlapsBody(const FScan& Scan, const FEventActorState& Actor)
		{
			const int32 Point = Whole(Actor.Position[1]) + Actor.Word62;
			return Point >= Scan.Head && Scan.Feet >= Point - Actor.Height;
		}

		void CheckBox(FScan& Scan, int32 Index, FEventActorState& Actor, FChoice& Choice)
		{
			if (!OverlapsBody(Scan, Actor) || Index == Scan.Context.LeaderActor || !IsInActorBox(Actor, Scan.X, Scan.Z, BoxMargin))
			{
				return;
			}
			if (WantsToTalk(Scan, Actor))
			{
				if ((Actor.Flags0 & TalkBlocked) != 0 || Scan.Context.Messages.OpenMask != 0)
				{
					return;
				}
				const int32 Angle = AngleTo(Scan, Actor);
				if ((Actor.Flags1 & TalksWhenFaced) == 0 || IsFacedForTalk(Scan, Actor, Angle))
				{
					ChooseTalk(Scan, Actor, Angle, Choice);
				}
			}
			else if ((Actor.Flags0 & TouchBlocked) == 0)
			{
				ChooseTouch(Actor, AngleTo(Scan, Actor), Choice);
			}
		}

		/** The squares as the GTE takes them: of each value's low 16 bits, summed in 32 bits. */
		int32 SquaredLength(int32 X, int32 Z)
		{
			return static_cast<int32>(static_cast<uint32>(GteSquare(X)) + static_cast<uint32>(GteSquare(Z)));
		}

		void CheckReach(FScan& Scan, int32 Index, FEventActorState& Actor, FChoice& Choice)
		{
			const int32 Dx = Whole(Actor.Position[0]) - Scan.X + Actor.InteractX;
			const int32 Dz = Whole(Actor.Position[2]) - Scan.Z + Actor.InteractZ;
			const int32 Distance = SquaredLength(Dx, Dz);
			const int32 TalkLimit = GteSquare(Scan.TalkRadius + Actor.Radius);
			if (Distance >= TalkLimit || !OverlapsBody(Scan, Actor) || Index == Scan.Context.LeaderActor)
			{
				return;
			}
			if (WantsToTalk(Scan, Actor))
			{
				if ((Actor.Flags0 & TalkBlocked) != 0)
				{
					return;
				}
				const int32 Angle = AngleTo(Scan, Actor);
				if (IsFacedForTalk(Scan, Actor, Angle) && Scan.Context.Messages.OpenMask == 0)
				{
					ChooseTalk(Scan, Actor, Angle, Choice);
				}
			}
			else if ((Actor.Flags0 & TouchBlocked) == 0 && Distance < GteSquare(Scan.TouchRadius + Actor.Radius))
			{
				ChooseTouch(Actor, AngleTo(Scan, Actor), Choice);
			}
		}
	}

	namespace
	{
		/** Whether the game set the actor up as a door: its init moved its interaction point to the middle of its panel. */
		bool IsDoor(const FEventActorState& Actor)
		{
			return Actor.InteractX != 0 || Actor.InteractZ != 0;
		}

		/** The way a heading walks, as a unit vector's x and z (4.12). */
		std::array<int64, 2> WalkWay(uint16 Heading)
		{
			return {GteCos(Heading & AngleMask), -GteSin(Heading & AngleMask)};
		}

		int64 Magnitude(int64 Value)
		{
			return Value < 0 ? -Value : Value;
		}

		/** The leader's step this tick, x and z in 20.12. */
		std::array<int64, 2> LeaderStep(const FEventContext& Context, const FEventActorState& Leader)
		{
			return {(int64{Leader.Position[0]} - Context.Controls.LeaderFrom[0]) >> 4, (int64{Leader.Position[2]} - Context.Controls.LeaderFrom[1]) >> 4};
		}

		/**
		 * Whether the leader's walk along Way was held up or turned aside this tick: it made no way along it, or left
		 * it sideways, as a slide along what it walks into does and a free walk does not.
		 */
		bool IsWalkHeldUp(const std::array<int64, 2>& Step, const std::array<int64, 2>& Way)
		{
			const int64 Along = Step[0] * Way[0] + Step[1] * Way[1];
			const int64 Aside = Step[0] * Way[1] - Step[1] * Way[0];
			return Along <= 0 || Magnitude(Aside) * HeldUpRatio > Along;
		}

		/**
		 * Whether the leader's step brought it nearer the line of the door's panel: a walk still on its way to the
		 * door, rather than one stopped before it or sliding along it.
		 */
		bool NearsDoor(const FEventActorState& Leader, const FEventActorState& Door, const std::array<int64, 2>& Step)
		{
			// Twelve fractional bits, from the panel's middle; the distances from the panel's line times the run's length.
			const int64 X = (int64{Leader.Position[0]} >> 4) - (int64{Whole(Door.Position[0]) + Door.InteractX} << 12);
			const int64 Z = (int64{Leader.Position[2]} >> 4) - (int64{Whole(Door.Position[2]) + Door.InteractZ} << 12);
			const int64 Now = Magnitude(X * Door.InteractZ - Z * Door.InteractX);
			const int64 Before = Magnitude((X - Step[0]) * Door.InteractZ - (Z - Step[1]) * Door.InteractX);
			const int64 Nearer = (Before - Now) * HeldUpRatio / DoorHalfWidth;
			return Nearer > 0 && Nearer * Nearer > Step[0] * Step[0] + Step[1] * Step[1];
		}

		/**
		 * Whether the leader walks at the door: the way it asks for, from where it stands, crosses the door's panel
		 * (its width, and the leader's own radius past either end) no farther ahead than a talk reaches.
		 */
		bool WalksAtDoor(const FScan& Scan, const FEventActorState& Leader, const FEventActorState& Door, const std::array<int64, 2>& Way)
		{
			// From the leader to the panel's middle, and the panel's run from its middle to its end.
			const int64 MiddleX = Whole(Door.Position[0]) + Door.InteractX - Scan.X;
			const int64 MiddleZ = Whole(Door.Position[2]) + Door.InteractZ - Scan.Z;
			const int64 RunX = Door.InteractX;
			const int64 RunZ = Door.InteractZ;
			// The leader + Ahead * Way = the middle + Along * Run, each as a quotient by Across.
			int64 Across = Way[0] * RunZ - Way[1] * RunX;
			int64 Ahead = (MiddleX * RunZ - MiddleZ * RunX) * 4096;
			int64 Along = MiddleX * Way[1] - MiddleZ * Way[0];
			if (Across < 0)
			{
				Across = -Across;
				Ahead = -Ahead;
				Along = -Along;
			}
			const int64 Reach = Scan.TalkRadius + Door.Radius;
			return Across != 0 && Ahead >= 0 && Ahead <= Reach * Across
				&& Magnitude(Along) * DoorHalfWidth <= Across * (DoorHalfWidth + Leader.Radius);
		}

		/**
		 * The door the leader has walked into long enough for it to open by itself, where the player's setting lets
		 * doors: the nearest door it walks at (a direction held whose way crosses the door's panel) while its walk is
		 * held up or turned aside and comes no nearer the panel, for DoorPushTicks ticks: stopped by the door or the
		 * floor's edge before it, or sliding along it. A push starts the door's script once: letting go or another
		 * direction lets the door answer again.
		 */
		const FEventActorState* FindPushedDoor(const FScan& Scan, const FEventActorState& Leader)
		{
			FFieldControls& Controls = Scan.Context.Controls;
			const bool bAsks = (Leader.Flags0 & EventActorFlags0::PadControlled) != 0 && (Leader.MoveDirection & NoDirection) == 0;
			if (!bAsks || Leader.MoveDirection != Controls.AnsweredDirection)
			{
				Controls.AnsweredDoor = -1;
			}
			int32 Nearest = -1;
			int32 NearestDistance = 0;
			const std::array<int64, 2> Way = WalkWay(Leader.MoveDirection);
			const std::array<int64, 2> Step = LeaderStep(Scan.Context, Leader);
			if (Scan.Context.bWalkIntoDoors && bAsks && IsWalkHeldUp(Step, Way))
			{
				for (std::size_t Index = 0; Index < Scan.Context.States.size(); ++Index)
				{
					const FEventActorState& Actor = Scan.Context.States[Index];
					if (!IsDoor(Actor) || (Actor.Flags0 & EventActorFlags0::Frozen) != 0 || (Actor.Flags1 & EventActorFlags1::NoTalkScript) != 0
						|| static_cast<int32>(Index) == Scan.Context.LeaderActor || !OverlapsBody(Scan, Actor) || !WalksAtDoor(Scan, Leader, Actor, Way)
						|| NearsDoor(Leader, Actor, Step))
					{
						continue;
					}
					const int32 Dx = Whole(Actor.Position[0]) - Scan.X + Actor.InteractX;
					const int32 Dz = Whole(Actor.Position[2]) - Scan.Z + Actor.InteractZ;
					const int32 Distance = Dx * Dx + Dz * Dz;
					if (Nearest < 0 || Distance < NearestDistance)
					{
						Nearest = static_cast<int32>(Index);
						NearestDistance = Distance;
					}
				}
			}
			if (Nearest != Controls.PushedDoor)
			{
				Controls.PushedDoor = static_cast<int16>(Nearest);
				Controls.PushedTicks = 0;
			}
			if (Nearest < 0)
			{
				return nullptr;
			}
			Controls.PushedTicks = static_cast<int16>(std::min<int32>(Controls.PushedTicks + 1, DoorPushTicks));
			const bool bOpens = Controls.PushedTicks >= DoorPushTicks && Nearest != Controls.AnsweredDoor;
			return bOpens ? &Scan.Context.States[static_cast<std::size_t>(Nearest)] : nullptr;
		}
	}

	bool CheckFieldTriggers(FEventContext& Context)
	{
		const FEventActorState* Leader = Context.StateOf(Context.LeaderActor);
		if (Leader == nullptr || Context.Arctangents == nullptr)
		{
			return false;
		}
		FScan Scan{Context};
		Scan.X = Whole(Leader->Position[0]);
		Scan.Z = Whole(Leader->Position[2]);
		Scan.Feet = Whole(Leader->Position[1]);
		Scan.Head = Scan.Feet - Leader->Height;
		Scan.TouchRadius = Leader->Radius + TouchReach;
		Scan.TalkRadius = Leader->Radius + TalkReach;
		Scan.Facing = Leader->Facing & AngleMask;
		Scan.Asked = Leader->MoveDirection;
		Scan.Pushed = FindPushedDoor(Scan, *Leader);
		const uint8 StoodOn = Leader->LinkedActor;
		for (std::size_t Index = 0; Index < Context.States.size(); ++Index)
		{
			FEventActorState& Actor = Context.States[Index];
			const int32 Number = static_cast<int32>(Index);
			if ((Actor.Flags0 & EventActorFlags0::Frozen) != 0 || StoodOn == Number)
			{
				continue;
			}
			FChoice Choice;
			if ((Actor.Flags1 & PlatformBits) != 0)
			{
				CheckPlatform(Scan, Actor, Choice);
			}
			if ((Actor.Flags0 & BoxShaped) != 0)
			{
				CheckBox(Scan, Number, Actor, Choice);
			}
			else
			{
				CheckReach(Scan, Number, Actor, Choice);
			}
			if (Choice.Script >= 0 && FEventMachine::StartScript(Context, Number, Choice.Script, Choice.Priority))
			{
				Actor.Facing |= TurnSet;
				Actor.MoveDirection = Actor.Facing;
			}
		}
		return true;
	}
}
