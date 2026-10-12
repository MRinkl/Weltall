#include "BattlingFightOps.h"
#include "BattlingMath.h"

namespace Weltall
{
	FBattlingMove FBattlingFighter::CurrentMove() const
	{
		// The round's first move is the program's empty record, whose trail flag the reset sets.
		return MoveState < 0 ? FBattlingMove{0, 0, 1, 0} : File->Moves[static_cast<std::size_t>(MoveState)];
	}
}

namespace Weltall::BattlingFightOps
{
	namespace
	{
		using namespace BattlingMath;

		enum : uint8
		{
			KindOnce,
			KindLoop,
			KindHold,
			KindOnceNoNext,
		};

		constexpr uint8 NoNext = 0xFF;
		constexpr uint8 ElFenrir = 13;
		constexpr int32 ElFenrirHiddenJoint = 13;
		constexpr int32 EffectEventLimit = 20;
		constexpr int32 TutorialState = 7;
		constexpr int32 ReplayState = 4;
		constexpr uint8 EtherBulletAttack = 0x20;
		constexpr uint8 ProjectileAttack = 4;
		constexpr uint8 NoTrailBit = 0x40;
		constexpr uint8 FirstSweptEffect = 0x10;
		constexpr uint8 LastLineEffect = 0x1F;
		constexpr uint8 SweptEffect = 0x10;
		constexpr uint8 FirstTrailEffect = 0x20;

		enum : int32
		{
			SoundWalking = 7,
			SoundDashStart = 8,
			SoundDashing = 9,
			SoundSplash = 0x3A,
		};

		/** An effect's or attack's id: its side, joint and vertex, and whether it is an attack (0) or an effect (1). */
		uint32 MakeId(int32 Side, uint8 Joint, int16 Vertex, uint32 Kind)
		{
			return (Kind & 0x7F) | (static_cast<uint32>(Side & 1) << 7) | (static_cast<uint32>(Joint) << 24) | (static_cast<uint32>(static_cast<uint16>(Vertex)) << 8);
		}

		FBattlingPoint Midpoint(const FBattlingPoint& A, const FBattlingPoint& B)
		{
			return {(A[0] + B[0]) / 2, (A[1] + B[1]) / 2, (A[2] + B[2]) / 2};
		}

		void ReturnToBase(FBattlingFighter& Fighter)
		{
			Fighter.Position = Fighter.Base;
			Fighter.Animation = 0;
			Fighter.Movement = 0;
			Fighter.Action = BattlingAction::Free;
			Fighter.Flags |= BattlingFlags::NoBlend;
		}

		/** An attack event's frame: the trail or band of the swing, then (while the attack is live) its volume, projectile or Ether Bullet. */
		void AnimationAttack(FBattlingFight& Fight, FBattlingFighter& Fighter, const FBattlingEvent& Event, bool bLive)
		{
			const int32 Side = Fighter.SideIndex();
			const uint32 Id = MakeId(Side, Event.JointA, Event.VertexA, 0);
			const bool bTrail = Fighter.CurrentMove().Trail != 0 && (Event.What & NoTrailBit) == 0;
			FBattlingPoint A = Fighter.Skeleton.JointPosition(Event.JointA, Event.VertexA, Fighter.Position);
			FBattlingPoint B = A;
			const bool bOnePoint = Event.JointA == Event.JointB && Event.VertexA == Event.VertexB;
			if (bOnePoint)
			{
				if (bTrail)
				{
					AddTrail(Fight, Side, A, Id, 0);
				}
			}
			else
			{
				B = Fighter.Skeleton.JointPosition(Event.JointB, Event.VertexB, Fighter.Position);
				if (bTrail)
				{
					AddSwept(Fight, Side, A, B, Id);
				}
			}
			if (!bLive)
			{
				return;
			}
			FBattlingPracticeInfo& Info = Fight.Practice[static_cast<std::size_t>(Side)];
			FBattlingReplayRecord& Record = Fighter.Records[Fighter.Record];
			if (Event.What == EtherBulletAttack)
			{
				const bool bFired = HeatAdd(Fight, Fighter, Fighter.EtherHeat, 1);
				const bool bReplayFired = Fight.LatchedState == ReplayState && (Fighter.Records[Fighter.Replayed].YawFlags & 0x8000) != 0;
				if ((!bFired || Fight.LatchedState == ReplayState) && !bReplayFired)
				{
					SpawnEffect(Fight, A, 9);
					QueueClear(Fighter);
					Record.YawFlags &= 0x7FFF;
				}
				else
				{
					if (!bOnePoint)
					{
						A = Midpoint(A, B);
					}
					SpawnProjectile(Fight, Fighter, A, nullptr, 0, Fighter.Stats->Ether, Id);
					QueueClear(Fighter);
					Record.YawFlags |= 0x8000;
				}
				Info.LastFrame = static_cast<int16>(Fighter.Clock);
				Info.FirstFrame = static_cast<int16>(Fighter.Clock);
				Info.ShownMove = FBattlingPracticeInfo::EtherShown;
				Info.Move = FBattlingPracticeInfo::EtherShown;
				Info.Power = Fighter.Stats->Ether;
				return;
			}
			if (Info.ShownMove != Info.Move)
			{
				Info.FirstFrame = static_cast<int16>(Fighter.Clock);
				Info.ShownMove = Info.Move;
			}
			Info.LastFrame = static_cast<int16>(Fighter.Clock);
			const uint8 What = Event.What;
			if (What == ProjectileAttack)
			{
				SpawnProjectile(Fight, Fighter, A, nullptr, 1, static_cast<int16>(Fighter.AttackPower), Id);
				return;
			}
			if (What > EtherBulletAttack && What <= EtherBulletAttack + 6)
			{
				SpawnProjectile(Fight, Fighter, A, bOnePoint ? nullptr : &B, What - EtherBulletAttack, static_cast<int16>(Fighter.AttackPower), Id);
				return;
			}
			// A hit volume: the swing already live from an earlier pass moves on, else a free slot starts a new one.
			const uint8 Pass = static_cast<uint8>(Fight.Pass);
			const auto Set = [&](FBattlingHitVolume& Volume) {
				Volume.PreviousA = Volume.A;
				Volume.PreviousB = Volume.B;
				Volume.A = A;
				Volume.B = B;
				Volume.Joint = Event.JointA;
				Volume.Flags = static_cast<uint8>((Volume.Flags & ~2) | (bOnePoint ? 2 : 0));
				Volume.State = 2;
				Volume.Power = static_cast<uint8>(Fighter.AttackPower);
				Volume.MoveState = Fighter.MoveState;
				Volume.Kind = What;
				++Fight.VolumesSet;
			};
			for (FBattlingHitVolume& Volume : Fighter.Volumes)
			{
				if (Volume.State == 1 && Volume.Id == Id && Volume.Pass != Pass)
				{
					Set(Volume);
					Volume.Flags &= static_cast<uint8>(~1);
					Volume.Pass = Pass;
					return;
				}
			}
			for (FBattlingHitVolume& Volume : Fighter.Volumes)
			{
				if (Volume.State == 0)
				{
					Set(Volume);
					Volume.Id = Id;
					Volume.Flags |= 1;
					Volume.Pass = Pass;
					return;
				}
			}
		}

		/** An effect event: a sprite or a trail at a joint (or between two), a band or a line chain between two. */
		void AnimationEffect(FBattlingFight& Fight, FBattlingFighter& Fighter, const FBattlingEvent& Event)
		{
			const int32 Side = Fighter.SideIndex();
			const uint32 Id = MakeId(Side, Event.JointA, Event.VertexA, 1);
			FBattlingPoint A = Fighter.Skeleton.JointPosition(Event.JointA, Event.VertexA, Fighter.Position);
			const uint8 What = Event.What;
			if (What >= FirstSweptEffect && What <= LastLineEffect)
			{
				const FBattlingPoint B = Fighter.Skeleton.JointPosition(Event.JointB, Event.VertexB, Fighter.Position);
				if (What == SweptEffect)
				{
					AddSwept(Fight, Side, A, B, Id);
				}
				else
				{
					SpawnLineEffect(Fight, A, B, What);
				}
				return;
			}
			if (Event.JointA != Event.JointB || Event.VertexA != Event.VertexB)
			{
				A = Midpoint(A, Fighter.Skeleton.JointPosition(Event.JointB, Event.VertexB, Fighter.Position));
			}
			Fight.Effects.Colour = Fighter.Colour;
			if (What < FirstTrailEffect)
			{
				SpawnEffect(Fight, A, What);
			}
			else
			{
				AddTrail(Fight, Side, A, Id, What - FirstTrailEffect);
			}
		}
	}

	void ResetParts(FBattlingFighter& Fighter)
	{
		std::vector<FBattlingJoint>& Joints = Fighter.Skeleton.Joints;
		for (FBattlingJoint& Joint : Joints)
		{
			if (Joint.Part >= 0)
			{
				Joint.bHidden = false;
			}
		}
		for (const uint8 Joint : Fighter.File->HiddenJoints)
		{
			Joints[Joint].bHidden = true;
		}
		if (Fighter.Gear == ElFenrir)
		{
			Joints[ElFenrirHiddenJoint].bHidden = true;
		}
	}

	void Animate(FBattlingFight& Fight, FBattlingFighter& Fighter)
	{
		FBattlingReplayRecord& Record = Fighter.Records[Fighter.Record];
		Record.YawFlags = static_cast<uint16>((Record.YawFlags & 0xF000) | (static_cast<uint32>(Fighter.Yaw) & 0xFFF));
		Record.X = static_cast<int16>(Fighter.Position[0]);
		Record.Y = static_cast<int16>(Fighter.Position[1]);
		Record.Z = static_cast<int16>(Fighter.Position[2]);
		uint8 RecordedBlend = 0;
		Record.Animation = Fighter.Animation;
		Record.YawFlags &= static_cast<uint16>(~0x1000);
		const FBattlingGearFile& File = *Fighter.File;
		if (Fighter.Animation != Fighter.LastAnimation || (Fighter.Flags & BattlingFlags::RestartLoop) != 0)
		{
			const uint32 Before = Fighter.Flags;
			Fighter.LastAnimation = Fighter.Animation;
			Fighter.Flags = (Before & ~BattlingFlags::AnimationEnded) | BattlingFlags::RestartAnimation;
			const std::array<uint8, 2>& Entry = File.AnimationTable[Fighter.Animation];
			if ((Before & BattlingFlags::RestartLoop) == 0)
			{
				Fighter.Blend = Entry[0];
			}
			if ((Fighter.Flags & BattlingFlags::NoBlend) != 0)
			{
				Fighter.Blend = 1;
				Fighter.Flags &= ~BattlingFlags::NoBlend;
			}
			Fighter.AnimationSpeed = static_cast<uint8>(Over256(Entry[1] * static_cast<int16>(Fighter.SpeedScale)));
			ResetParts(Fighter);
			Fighter.Player(Fighter.Animation).Start(File, Fighter.Animation);
			Fighter.Clock = 0;
			Fighter.EventFrame = 0xFFFF;
			Fighter.Flags &= ~BattlingFlags::RestartLoop;
			Record.YawFlags |= 0x1000;
			Fighter.HeldAnimation = Fight.AnimationKinds[Fighter.Animation].Kind == KindHold ? Fighter.Animation : static_cast<uint8>(0);
		}
		const FBattlingAnimationKind& Kind = Fight.AnimationKinds[Fighter.Animation];
		FBattlingAnimationPlayer& Player = Fighter.Player(Fighter.Animation);
		const uint16 Old = Fighter.Clock;
		Fighter.Clock = static_cast<uint16>(Old + Fighter.AnimationSpeed + Fighter.SpeedBonus);
		const int32 Frames = (static_cast<int16>(Fighter.Clock) >> 4) - (static_cast<int16>(Old) >> 4);
		Fighter.FramesStepped = static_cast<int16>(Frames);
		Fighter.FrameBefore = Player.Frame();
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			if (Fighter.Blend != 1)
			{
				--Fighter.Blend;
				if (Fighter.Blend < 1)
				{
					Fighter.Blend = 1;
				}
			}
		}
		const auto Step = [&]() {
			const bool bDone = Player.Step(File, Fighter.Skeleton, Frames, Fighter.Blend);
			RecordedBlend = static_cast<uint8>(Fighter.Blend);
			Fighter.Flags = (Fighter.Flags & ~BattlingFlags::AnimationEnded) | (bDone ? BattlingFlags::AnimationEnded : 0);
			return bDone;
		};
		uint8 RecordedFrames = static_cast<uint8>(Frames);
		switch (Kind.Kind)
		{
		case KindLoop:
			Fighter.Flags &= ~BattlingFlags::RestartAnimation;
			if (Step())
			{
				Fighter.Flags |= BattlingFlags::RestartLoop;
				if (Kind.Next != NoNext)
				{
					Fighter.Animation = Kind.Next;
				}
			}
			break;
		case KindOnce:
			RecordedFrames = 0;
			if ((Fighter.Flags & BattlingFlags::AnimationEnded) == 0)
			{
				const uint32 Before = Fighter.Flags;
				RecordedFrames = static_cast<uint8>(Frames);
				if (Step())
				{
					if (Kind.Next == NoNext)
					{
						Fighter.Flags = (Before & ~(BattlingFlags::AnimationEnded | BattlingFlags::RestartAnimation)) | BattlingFlags::AnimationEnded;
					}
					else
					{
						Fighter.Animation = Kind.Next;
					}
				}
			}
			break;
		case KindHold:
		{
			const uint32 Before = Fighter.Flags;
			Fighter.Flags = Before & ~BattlingFlags::RestartAnimation;
			RecordedFrames = 0;
			if ((Before & BattlingFlags::AnimationEnded) == 0)
			{
				Step();
				RecordedFrames = static_cast<uint8>(Frames);
			}
			if ((Fighter.Flags & BattlingFlags::ReleaseHeld) != 0)
			{
				if (Kind.Next == NoNext)
				{
					Fighter.Flags &= ~BattlingFlags::RestartAnimation;
				}
				else
				{
					Fighter.Animation = Kind.Next;
				}
				Fighter.Flags &= ~BattlingFlags::ReleaseHeld;
			}
			break;
		}
		case KindOnceNoNext:
		default:
			RecordedFrames = 0;
			if ((Fighter.Flags & BattlingFlags::AnimationEnded) == 0)
			{
				Step();
				RecordedFrames = static_cast<uint8>(Frames);
			}
			break;
		}
		Record.Frames = RecordedFrames;
		Record.Blend = RecordedBlend;
	}

	void RecordFlags(FBattlingFighter& Fighter)
	{
		FBattlingReplayRecord& Record = Fighter.Records[Fighter.Record];
		Record.bGuard = (Fighter.Flags & BattlingFlags::GuardActive) != 0;
		Record.YawFlags = static_cast<uint16>((Record.YawFlags & ~0x2000) | ((Fighter.Flags >> 2) & 0x2000));
		Record.YawFlags = static_cast<uint16>((Record.YawFlags & ~0x4000) | ((Fighter.Flags >> 5) & 0x4000));
	}

	void UpdateBody(FBattlingFight& Fight, FBattlingFighter& Fighter)
	{
		const FBattlingGearFile& File = *Fighter.File;
		const FBattlingPoint FootA = Fighter.Skeleton.JointPosition(File.FootJoints[0], File.FootVertices[0], Fighter.Position);
		const FBattlingPoint FootB = Fighter.Skeleton.JointPosition(File.FootJoints[1], File.FootVertices[1], Fighter.Position);
		Fighter.Base = Midpoint(FootA, FootB);
		const bool bInWater = Fighter.Ground() == EBattlingGround::Water;
		if ((Fighter.Flags & BattlingFlags::BackDashDust) != 0 && !bInWater && (Fighter.Kind & 1) == 0)
		{
			SpawnEffect(Fight, FootA, 9);
			SpawnEffect(Fight, FootB, 9);
		}
		if (Fighter.SandDust != 0)
		{
			if (Fighter.GroundHeight - 0x20 < FootA[1] + 0x40 && Fighter.FootHeights[1] + 6 < FootA[1])
			{
				SpawnDust(Fight, FootA, 0x20);
			}
			if (Fighter.GroundHeight - 0x20 < FootB[1] + 0x40 && Fighter.FootHeights[0] + 6 < FootB[1])
			{
				SpawnDust(Fight, FootB, 0x20);
			}
			--Fighter.SandDust;
		}
		if (bInWater)
		{
			SpawnSplashes(Fight, Fighter.Base, Fighter.PreviousBase);
		}
		if (Fight.LatchedState != TutorialState && bInWater && 0x80 < Fighter.Base[1] && Fighter.PreviousBase[1] < 0x80)
		{
			Fight.Sound.PlayAt(Fighter.SideIndex(), SoundSplash, &Fighter.Base, EBattlingSoundPlace::Following);
		}
		Fighter.FootHeights[1] = static_cast<int16>(FootA[1]);
		Fighter.FootHeights[0] = static_cast<int16>(FootB[1]);
		Fighter.Top = Fighter.Skeleton.JointPosition(File.TopJoint, File.TopVertex, Fighter.Position);
		Fighter.Top[1] -= 0x38;
		Fighter.Centre = Midpoint(Fighter.Base, Fighter.Top);
		if ((Fighter.Flags & BattlingFlags::DashInput) != 0 && Fighter.Action == BattlingAction::Free && (Fighter.Movement & BattlingMovement::Moving) != 0)
		{
			Fight.Effects.Colour = Fighter.Colour;
			const int32 Side = Fighter.SideIndex();
			AddSwept(Fight, Side, Fighter.Top, Fighter.Base, static_cast<uint32>(Side));
			if ((Fighter.Flags & BattlingFlags::LastDashInput) == 0)
			{
				StopFighterSound(Fight, Fighter, SoundDashStart);
				PlayFighterSound(Fight, Fighter, SoundDashStart, &Fighter.Base, EBattlingSoundPlace::Following);
			}
		}
		if (Fighter.Action != BattlingAction::BackDashing)
		{
			uint32 Sound = Fighter.Flags2 & ~BattlingFlags2::MovementSound;
			if (Fighter.Movement == BattlingMovement::Moving)
			{
				Fighter.Flags2 = Sound | 1;
				if ((Fighter.Flags & BattlingFlags::DashInput) != 0)
				{
					Fighter.Flags2 = Sound | 2;
				}
			}
			else
			{
				Fighter.Flags2 = Sound;
			}
			if (Fighter.Action == BattlingAction::Attacking)
			{
				Fighter.Flags2 |= 3;
			}
		}
		const uint32 Sound = Fighter.Flags2 & 3;
		if (Sound != ((Fighter.Flags2 >> 2) & 3))
		{
			switch (Sound)
			{
			case 1:
				PlayFighterSound(Fight, Fighter, SoundWalking, &Fighter.Position, EBattlingSoundPlace::Following);
				StopFighterSound(Fight, Fighter, SoundDashing);
				break;
			case 2:
				PlayFighterSound(Fight, Fighter, SoundDashing, &Fighter.Base, EBattlingSoundPlace::Following);
				StopFighterSound(Fight, Fighter, SoundWalking);
				break;
			default:
				StopFighterSound(Fight, Fighter, SoundWalking);
				StopFighterSound(Fight, Fighter, SoundDashing);
				break;
			}
		}
		Fighter.Flags = (Fighter.Flags & ~BattlingFlags::LastDashInput) | (((Fighter.Flags >> 15) & 1) << 16);
		Fighter.LastMovement = Fighter.Movement;
		Fighter.Flags2 = (Fighter.Flags2 & ~BattlingFlags2::LastMovementSound) | ((Fighter.Flags2 & 3) << 2);
	}

	void RunAnimationEvents(FBattlingFight& Fight, FBattlingFighter& Fighter, int32 Frame, int32 Count, int32 EffectsRun)
	{
		if ((Count & 0xFFFF) == 0)
		{
			Count = 1;
		}
		if (Fighter.EventFrame == static_cast<uint16>(Frame))
		{
			return;
		}
		Fighter.EventFrame = static_cast<uint16>(Frame);
		const FBattlingGearFile& File = *Fighter.File;
		const std::vector<FBattlingEventSpan>& List = File.EventLists[Fighter.Animation];
		if (File.HasEventList[Fighter.Animation])
		{
			bool bSoundsPlayed = false;
			Fight.VolumesSet = 0;
			std::array<uint16, EffectEventLimit> EffectsDone{};
			int16 Remaining = static_cast<int16>(Count - 1);
			while (Remaining != -1)
			{
				const int16 Current = static_cast<int16>(Frame);
				for (const FBattlingEventSpan& Span : List)
				{
					if (Current < Span.First || Span.Last < Current)
					{
						continue;
					}
					const FBattlingEvent& Event = File.Events[Span.Event];
					switch (Event.Kind)
					{
					case EBattlingEventKind::Attack:
						if (static_cast<uint16>(Frame) == Span.First)
						{
							Fighter.Flags |= BattlingFlags::AttackLive;
						}
						AnimationAttack(Fight, Fighter, Event, (Fighter.Flags & BattlingFlags::AttackLive) != 0);
						if (static_cast<uint16>(Frame) == Span.Last)
						{
							Fighter.Flags &= ~BattlingFlags::AttackLive;
						}
						break;
					case EBattlingEventKind::Sounds:
						if (!bSoundsPlayed)
						{
							bSoundsPlayed = true;
							Fight.Sound.PlayAt(Fighter.SideIndex(), Event.JointA, &Fighter.Position, EBattlingSoundPlace::Following);
							Fight.Sound.PlayAt(Fighter.SideIndex(), Event.JointB, &Fighter.Position, EBattlingSoundPlace::Following);
						}
						break;
					case EBattlingEventKind::Effect:
					{
						bool bDone = false;
						for (int32 Index = 0; Index < EffectsRun && Index < EffectEventLimit; ++Index)
						{
							bDone = bDone || EffectsDone[static_cast<std::size_t>(Index)] == Span.Event;
						}
						if (!bDone && EffectsRun < EffectEventLimit)
						{
							EffectsDone[static_cast<std::size_t>(EffectsRun)] = Span.Event;
							++EffectsRun;
							AnimationEffect(Fight, Fighter, Event);
						}
						break;
					}
					case EBattlingEventKind::ReturnToBase:
						ReturnToBase(Fighter);
						break;
					case EBattlingEventKind::HidePart:
						Fighter.Skeleton.Joints[Event.What].bHidden = true;
						break;
					case EBattlingEventKind::ShowPart:
						Fighter.Skeleton.Joints[Event.What].bHidden = false;
						break;
					}
				}
				Remaining = static_cast<int16>(Remaining - 1);
				++Frame;
			}
		}
		AgeVolumes(Fighter);
	}
}
