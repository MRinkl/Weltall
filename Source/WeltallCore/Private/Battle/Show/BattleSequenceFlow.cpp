#include "BattleObjectSequence.h"

#include "Game/PsxRandom.h"

#include <string>

namespace Weltall::BattleObjectMachine
{
	namespace
	{
		constexpr uint32 TracksRunning = 0x100;
		constexpr uint32 Looped = 0x400;
		constexpr uint32 WatchedRunning = 1;
		constexpr uint32 WatchedLooped = 4;
		constexpr uint8 AnyTrack = 0xFF;
		/** The start selector that also runs the running object's own new sequence at once, ending this run. */
		constexpr uint8 ItselfAtOnce = 0xFD;
		constexpr uint8 EachItsOwn = 0xFF;
		constexpr uint8 QueueOn = 1;
		constexpr uint8 QueueFlush = 2;
		constexpr uint8 NoSequence = 0xFF;
		constexpr int32 HalfOfRand = 0x3FFF;
		/** The slots a mask of op 14, 25, 26 and 3E covers: the combatants, the special model and the slot after it. */
		constexpr int32 MaskedSlots = 13;
		/** Op 3E's class that asks whether each target reacts. */
		constexpr uint8 ReactsClass = 8;
		/** PeekCode's answer when no record follows. */
		constexpr int32 NoMoreRecords = -2;

		bool EventsReset(const FSequenceRun& Run)
		{
			return Run.Events == -1;
		}

		void Stop(FBattleObjects&, FSequenceRun& Run, FBattleObject&)
		{
			Run.Stop();
		}

		void Wait(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			if (!EventsReset(Run))
			{
				const int16 Ticks = SignedWord(Objects, Run, 1);
				Object.WaitTicks = static_cast<uint16>(Object.WaitTicks + Run.Ticks);
				if (Ticks <= static_cast<int16>(Object.WaitTicks))
				{
					Object.WaitTicks = 0;
					Run.Ticks = 0;
					Run.Next = Run.At + 4;
					return;
				}
			}
			Run.Stop();
		}

		/** The sequence's end signalled to the event once the numbers are gone. */
		void SignalEnd(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			if (Objects.Services->NumbersPending())
			{
				Run.Stop();
				return;
			}
			Objects.Services->StopCountingTasks();
			Object.Signal = 0;
			Objects.Services->SignalEventPerform(Object.Slot);
		}

		void SignalOrCount(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			if (Object.Signal == 0)
			{
				Run.bCountRecord = true;
				return;
			}
			SignalEnd(Objects, Run, Object);
		}

		void SignalIfAsked(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			if (Object.Signal != 0)
			{
				SignalEnd(Objects, Run, Object);
			}
		}

		void StartSequences(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const uint16 Operand = Word(Objects, Run, 1);
			Run.Next = Run.At + 4;
			uint16 Mask = 0;
			const uint32 Owner = ResolveWho(Objects, Object, Operand & 0xFF, Mask);
			ResolveWho(Objects, Object, Run.B, Mask);
			const uint8 Queue = Object.Queue;
			const int32 Slot = Objects.PlaceOf(Object);
			if (Run.B == ItselfAtOnce)
			{
				Object.Queue = 0;
			}
			for (int32 Masked = 0; Masked < MaskedSlots && !Objects.Faulted(); ++Masked)
			{
				if (((static_cast<int16>(Mask) >> Masked) & 1) == 0)
				{
					continue;
				}
				int32 OwnerSlot = Masked;
				if ((Operand & 0xFF) != EachItsOwn)
				{
					if (NamedObject(Objects, Owner & 0xFF) == nullptr)
					{
						continue;
					}
					OwnerSlot = static_cast<int32>(Owner & 0xFF);
				}
				Objects.StartSequence(Masked, OwnerSlot, Operand >> 8);
			}
			if (FBattleObject* Running = Objects.Object(Slot))
			{
				Running->Queue = Queue;
			}
			if (Run.B == ItselfAtOnce)
			{
				Run.bEnd = true;
			}
		}

		void CloneOp(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const int16 Bone = SignedWord(Objects, Run, 1);
			Run.Next = Run.At + 4;
			Clone(Objects, Objects.PlaceOf(Object), Bone, Run.B);
		}

		void Merge(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			FBattleObject* Owner = Objects.Object(Object.Owner);
			if (Owner == nullptr)
			{
				Objects.Fail("a clone merges back into the empty slot " + std::to_string(Object.Owner));
				return;
			}
			for (std::size_t Bone = 1; Bone < Object.Bones.size(); ++Bone)
			{
				if (Object.Bones[Bone].bShown != 0)
				{
					Object.Bones[Bone].bShown = 0;
					if (Bone >= Owner->Bones.size())
					{
						Objects.Fail("a clone shows bone " + std::to_string(Bone) + " of an owner without it");
						return;
					}
					Owner->Bones[Bone].bShown = 1;
				}
			}
			const int32 OwnerSlot = Object.Owner;
			Objects.Free(Object.Slot);
			if (Run.B != NoSequence)
			{
				Objects.StartSequence(OwnerSlot, OwnerSlot, Run.B);
			}
			Run.bEnd = true;
		}

		void FreeSelf(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			Objects.Free(Object.Slot);
			Run.bEnd = true;
		}

		void SwitchObject(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject&)
		{
			const FBattleObject* First = Objects.Object(Run.FirstSlot);
			if (First == nullptr)
			{
				Objects.Fail("a sequence switches objects after its first is freed");
				return;
			}
			uint16 Mask = 0;
			const uint32 Slot = ResolveWho(Objects, *First, Run.B, Mask) & 0xFF;
			if (NamedObject(Objects, Slot) != nullptr)
			{
				Run.Slot = static_cast<int32>(Slot);
			}
		}

		void WaitTracks(FBattleObjects&, FSequenceRun& Run, FBattleObject&)
		{
			if (EventsReset(Run) || (static_cast<uint32>(Run.Events) & TracksRunning) != 0)
			{
				Run.Stop();
			}
		}

		void WaitTag(FBattleObjects&, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.WatchTag = Run.B;
			if (EventsReset(Run) || (static_cast<uint32>(Run.Events) & WatchedRunning) != 0)
			{
				Run.Stop();
			}
		}

		void WaitLoops(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			if (!EventsReset(Run))
			{
				const int16 Count = SignedWord(Objects, Run, 1);
				Run.Next = Run.At + 4;
				uint32 Wanted = Looped;
				if (Run.B != AnyTrack)
				{
					Object.WatchTag = Run.B;
					Wanted = WatchedLooped;
				}
				if ((static_cast<uint32>(Run.Events) & Wanted) != 0)
				{
					++Object.Loops;
					if (Count <= static_cast<int32>(Object.Loops))
					{
						Object.Loops = 0;
						return;
					}
				}
			}
			Run.Stop();
		}

		/** 28, 29: on bone Lo's track reaching step Hi, the target's distance in steps below B (28) or not (29). */
		template <bool bNear>
		void WaitFrame(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const int32 Far = TargetDistance(Objects, Object);
			if (Object.Speed == 0)
			{
				Object.Speed = 1;
			}
			const uint16 Operand = Word(Objects, Run, 1);
			Run.Next = Run.At + 4;
			const FGearBone* Bone = GearMachine::BoneAt(Objects, Object, Operand & 0xFF);
			if (Bone == nullptr)
			{
				return;
			}
			const int16 Track = Bone->Rotation != FGearBone::NoChannel ? Bone->Rotation : Bone->Translation;
			const bool bAtStep = Track != FGearBone::NoChannel && Objects.Channels[Track].Tick == static_cast<int32>(Operand >> 8);
			const int32 Steps = GearMachine::Divide(Objects, Far, Object.Speed);
			if (!bAtStep || (bNear ? static_cast<int32>(Run.B) <= Steps : Steps < static_cast<int32>(Run.B)))
			{
				Run.Stop();
			}
		}

		template <bool bNear>
		void WaitDistance(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const int32 Far = TargetDistance(Objects, Object);
			if (bNear ? Object.Speed <= Far : Far <= Object.Speed)
			{
				Run.Stop();
			}
		}

		/** 2C, 2D: the distance to a point within the approach distance (2C) or beyond it (2D). */
		template <bool bNear>
		void WaitPoint(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const FGearBone& Root = Object.Bones[0];
			const int32 X = SignedWord(Objects, Run, 1) - Root.Position[0];
			const int32 Y = SignedWord(Objects, Run, 2) - Root.Position[1];
			const int32 Z = SignedWord(Objects, Run, 3) - Root.Position[2];
			Run.Next = Run.At + 8;
			const int32 Far = GearMachine::Distance(Objects, X, Y, Z);
			if (bNear ? Object.Speed <= Far : Far <= Object.Speed)
			{
				Run.Stop();
			}
		}

		FGearCodeRef Handler(FBattleObjects& Objects, const FSequenceRun& Run, int32 OffsetWord)
		{
			const int16 Offset = SignedWord(Objects, Run, OffsetWord);
			return Run.B == 0 ? FGearCodeRef{} : Run.At + Offset;
		}

		void OnNear(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.NearDistance = Object.Speed;
			Object.OnNear = Handler(Objects, Run, 1);
			Run.Next = Run.At + 4;
		}

		void WaitNumbers(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject&)
		{
			if (Objects.Services->NumbersPending())
			{
				Run.Stop();
			}
		}

		void LoopStart(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject&)
		{
			GearMachine::WriteU16(Objects, Run.Word(1), 0);
			Run.Next = Run.At + 4;
		}

		void LoopEnd(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject&)
		{
			const FGearCodeRef Start = JumpTarget(Objects, Run);
			const int32 Count = GearMachine::ReadU16(Objects, Start) >> 8;
			const uint16 Done = static_cast<uint16>(GearMachine::ReadU16(Objects, Start + 2) + 1);
			GearMachine::WriteU16(Objects, Start + 2, Done);
			Run.Next = static_cast<int16>(Done) < Count ? Start + 4 : Run.At + 4;
		}

		void Jump(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject&)
		{
			Run.Next = JumpTarget(Objects, Run);
		}

		/** A jump taken when bTaken, past the word otherwise. */
		void JumpIf(FBattleObjects& Objects, FSequenceRun& Run, bool bTaken)
		{
			const FGearCodeRef Target = JumpTarget(Objects, Run);
			Run.Next = bTaken ? Target : Run.At + 4;
		}

		void JumpInGearBattle(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject&)
		{
			JumpIf(Objects, Run, Objects.BattleKind != 0);
		}

		void JumpUnlessFromFiles(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			JumpIf(Objects, Run, !Object.bFromFiles);
		}

		void JumpRandom(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject&)
		{
			const FGearCodeRef Target = JumpTarget(Objects, Run);
			Run.Next = Run.At + 4;
			if (Objects.Random == nullptr)
			{
				Objects.Fail("the battle's objects have no random numbers to draw");
				return;
			}
			if (Objects.Random->Next() > HalfOfRand)
			{
				Run.Next = Target;
			}
		}

		void OnTimer(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.Timer = 0;
			Object.TimerEnd = Word(Objects, Run, 1);
			Object.OnTimer = Handler(Objects, Run, 2);
			Run.Next = Run.At + 6;
		}

		void OnLanding(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.OnLanding = Handler(Objects, Run, 1);
			Run.Next = Run.At + 4;
		}

		/** 3A: the numbers waited for unless another record follows for a model made from files that signals nothing. */
		void WaitNextRecord(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const std::optional<int32> Code = PeekCode(Objects, 2);
			if (!Code)
			{
				return;
			}
			if (*Code == NoMoreRecords || !Object.bFromFiles || Object.Signal != 0)
			{
				WaitNumbers(Objects, Run, Object);
			}
		}

		void JumpIfKnockedOut(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			JumpIf(Objects, Run, ((KnockedOutSlots(Objects) >> (Object.Slot & 0x1F)) & 1) != 0);
		}

		void FlushIfQueued(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const uint8 Queued = Object.Queue;
			for (uint8 Index = 1; Index < Queued; ++Index)
			{
				if (Object.QueuedSequences[Index - 1u] == Run.B)
				{
					FlushQueue(Objects, Run, Object, Queued);
					return;
				}
			}
		}

		/** 3E: a jump when every target of the play shows class B (0 hit, 5 heal or half, 4 miss) or, for 8, reacts. */
		void JumpIfTargetsAre(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject&)
		{
			bool bAll = true;
			for (int32 Slot = 0; Slot < MaskedSlots && !Objects.Faulted(); ++Slot)
			{
				if (((Objects.PlayTargets >> Slot) & 1) == 0)
				{
					continue;
				}
				const std::optional<uint8> Result = ResultClass(Objects, Slot);
				if (!Result)
				{
					return;
				}
				uint8 Class = 4;
				if (*Result <= 1)
				{
					Class = 0;
				}
				else if (*Result == 2 || *Result == 3 || *Result == 5)
				{
					Class = 5;
				}
				if (Class != Run.B && Run.B < ReactsClass)
				{
					bAll = false;
				}
				const FBattleObject* Target = Objects.Object(Slot);
				if (Run.B == ReactsClass && (Target == nullptr || (Target->Flags & BattleObjectFlags::Reacts) == 0))
				{
					bAll = false;
				}
			}
			JumpIf(Objects, Run, bAll);
		}

		void Queue(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const uint8 Queued = Object.Queue;
			if (Run.B == QueueOn && Queued >= 2)
			{
				return;
			}
			Object.Queue = Run.B;
			if (Run.B == QueueFlush)
			{
				FlushQueue(Objects, Run, Object, Queued);
			}
		}

		void JumpAtTarget(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const FGearBone& Root = Object.Bones[0];
			JumpIf(Objects, Run, Object.Target[0] == Root.Position[0] && Object.Target[1] == Root.Position[1] && Object.Target[2] == Root.Position[2]);
		}

		void JumpIfGroupHasRoom(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const std::optional<uint8> Count = GearGroupCount(Objects, Object.Slot);
			if (Count)
			{
				JumpIf(Objects, Run, *Count != 0);
			}
		}

		/** The port's files are in memory: no read is ever pending. */
		void WaitFiles(FBattleObjects&, FSequenceRun&, FBattleObject&)
		{
		}

		void SetPlaying(FBattleObjects&, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.Playing = Run.B & 1;
		}

		void WaitPlaying(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			uint16 Mask = 0;
			const uint32 Slot = ResolveWho(Objects, Object, Run.B, Mask) & 0xFF;
			const uint16 Operand = Word(Objects, Run, 1);
			Run.Next = Run.At + 4;
			const FBattleObject* Other = NamedObject(Objects, Slot);
			if (Other != nullptr && Other->Playing == (Operand & 1))
			{
				Run.Stop();
			}
		}

		void MarkTargets(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.Mark = Run.B == 0 ? int16{-1} : static_cast<int16>(Objects.PlayTargets);
		}

		void JumpIfMarked(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const FGearCodeRef Target = JumpTarget(Objects, Run);
			Run.Next = Run.At + 4;
			if (static_cast<int32>(Object.Mark) == static_cast<int32>(Objects.PlayTargets))
			{
				Run.Stop(Target);
			}
		}

		void JumpIfFacing(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const FGearBone& Root = Object.Bones[0];
			const int32 Yaw = GearMachine::Ratan2(Objects, Root.Position[0] - Object.Target[0], Root.Position[2] - Object.Target[2]);
			JumpIf(Objects, Run, (Yaw & 0xFFF) == (static_cast<uint16>(Root.Angles[1]) & 0xFFF));
		}
	}

	void AddSequenceFlowOps(std::array<FSequenceOp, SequenceOpCount>& Ops)
	{
		Ops[0x00] = &Stop;
		Ops[0x01] = &Wait;
		Ops[0x02] = &SignalOrCount;
		Ops[0x03] = &SignalIfAsked;
		Ops[0x14] = &StartSequences;
		Ops[0x15] = &CloneOp;
		Ops[0x16] = &Merge;
		Ops[0x17] = &FreeSelf;
		Ops[0x1F] = &SwitchObject;
		Ops[0x20] = &WaitTracks;
		Ops[0x21] = &WaitTag;
		Ops[0x22] = &WaitLoops;
		Ops[0x28] = &WaitFrame<true>;
		Ops[0x29] = &WaitFrame<false>;
		Ops[0x2A] = &WaitDistance<true>;
		Ops[0x2B] = &WaitDistance<false>;
		Ops[0x2C] = &WaitPoint<true>;
		Ops[0x2D] = &WaitPoint<false>;
		Ops[0x2E] = &OnNear;
		Ops[0x2F] = &WaitNumbers;
		Ops[0x30] = &LoopStart;
		Ops[0x31] = &LoopEnd;
		Ops[0x32] = &Jump;
		Ops[0x33] = &JumpInGearBattle;
		Ops[0x34] = &JumpUnlessFromFiles;
		Ops[0x35] = &JumpRandom;
		Ops[0x36] = &OnTimer;
		Ops[0x37] = &OnLanding;
		Ops[0x3A] = &WaitNextRecord;
		Ops[0x3B] = &JumpIfKnockedOut;
		Ops[0x3D] = &FlushIfQueued;
		Ops[0x3E] = &JumpIfTargetsAre;
		Ops[0x5B] = &Queue;
		Ops[0x5C] = &JumpAtTarget;
		Ops[0x61] = &JumpIfGroupHasRoom;
		Ops[0x6C] = &WaitFiles;
		Ops[0x6D] = &SetPlaying;
		Ops[0x6E] = &WaitPlaying;
		Ops[0x6F] = &MarkTargets;
		Ops[0x70] = &JumpIfMarked;
		Ops[0x75] = &JumpIfFacing;
	}
}
