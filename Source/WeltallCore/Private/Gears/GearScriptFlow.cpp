#include "GearScript.h"

#include "Game/PsxRandom.h"

#include <string>

namespace Weltall::GearMachine
{
	namespace
	{
		constexpr uint32 ChannelsRunning = 0x100;
		constexpr uint32 Looped = 0x400;
		constexpr uint32 WatchedRunning = 1;
		constexpr uint32 WatchedLooped = 4;
		constexpr uint8 AnyChannel = 0xFF;
		/** The start selector that also runs the instruction's own model's new script at once, ending this run. */
		constexpr uint8 OwnSlotAtOnce = 0xFD;
		constexpr uint8 EachModelItself = 0xFF;
		constexpr uint8 QueueOn = 1;
		constexpr uint8 QueueFlush = 2;
		constexpr int32 HalfOfRand = 0x3FFF;

		bool EventsReset(const FGearScriptRun& Run)
		{
			return Run.Events == -1;
		}

		void Wait(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			if (!EventsReset(Run))
			{
				const int16 Ticks = ReadS16(Models, Run.Word(1));
				Model.WaitTicks = static_cast<uint16>(Model.WaitTicks + Run.Ticks);
				if (Ticks <= static_cast<int16>(Model.WaitTicks))
				{
					Model.WaitTicks = 0;
					Run.Ticks = 0;
					Run.Next = Run.At + 4;
					return;
				}
			}
			Run.Stop();
		}

		/** The field's hook for these (a function that returns at once): nothing to do. */
		void Notify(FGearModels&, FGearScriptRun&, FGearModel&)
		{
		}

		void StartScripts(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			const uint16 Word = ReadU16(Models, Run.Word(1));
			Run.Next = Run.At + 4;
			uint16 Mask = 0;
			const uint32 Owner = SelectModel(Models, Model, Word & 0xFF, Mask);
			SelectModel(Models, Model, Run.B, Mask);
			const uint8 Queue = Model.Queue;
			const int32 Slot = Run.Slot;
			if (Run.B == OwnSlotAtOnce)
			{
				Model.Queue = 0;
			}
			for (int32 Masked = 0; Masked < 8 && !Models.Faulted(); ++Masked)
			{
				if (((Mask >> Masked) & 1) != 0 && Models.Model(Masked) != nullptr)
				{
					StartScript(Models, Masked, (Word & 0xFF) == EachModelItself ? Masked : static_cast<int32>(Owner & 0xFF), Word >> 8);
				}
			}
			if (FGearModel* Running = Models.Model(Slot))
			{
				Running->Queue = Queue;
			}
			if (Run.B == OwnSlotAtOnce)
			{
				Run.bEnd = true;
			}
		}

		void Clone(FGearModels& Models, FGearScriptRun&, FGearModel&)
		{
			Models.Fail("a gear's script clones part of its model, whose clone shares scale channels its model frees");
		}

		void FreeSelf(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			Models.Free(Model.Slot);
			Run.bEnd = true;
		}

		void SwitchModel(FGearModels& Models, FGearScriptRun& Run, FGearModel&)
		{
			FGearModel* First = Models.Model(Run.FirstSlot);
			if (First == nullptr)
			{
				Models.Fail("a gear's script switches models after its first is freed");
				return;
			}
			uint16 Mask = 0;
			const uint32 Slot = SelectModel(Models, *First, Run.B, Mask) & 0xFF;
			if (Slot >= FGearModels::Slots)
			{
				Models.Fail("a gear's script switches to slot " + std::to_string(Slot) + ", past the models' slots");
				return;
			}
			if (Models.Model(static_cast<int32>(Slot)) != nullptr)
			{
				Run.Slot = static_cast<int32>(Slot);
			}
		}

		void WaitChannels(FGearModels&, FGearScriptRun& Run, FGearModel&)
		{
			if (EventsReset(Run) || (static_cast<uint32>(Run.Events) & ChannelsRunning) != 0)
			{
				Run.Stop();
			}
		}

		void WaitTag(FGearModels&, FGearScriptRun& Run, FGearModel& Model)
		{
			Model.WatchTag = Run.B;
			if (EventsReset(Run) || (static_cast<uint32>(Run.Events) & WatchedRunning) != 0)
			{
				Run.Stop();
			}
		}

		void WaitLoops(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			if (!EventsReset(Run))
			{
				const int16 Count = ReadS16(Models, Run.Word(1));
				Run.Next = Run.At + 4;
				uint32 Wanted = Looped;
				if (Run.B != AnyChannel)
				{
					Model.WatchTag = Run.B;
					Wanted = WatchedLooped;
				}
				if ((static_cast<uint32>(Run.Events) & Wanted) != 0)
				{
					++Model.Loops;
					if (Count <= static_cast<int32>(Model.Loops))
					{
						Model.Loops = 0;
						return;
					}
				}
			}
			Run.Stop();
		}

		/** 28, 29: on bone Lo's channel reaching tick Hi, the target's distance in steps below B (28) or not (29). */
		template <bool bNear>
		void WaitFrame(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			const int32 Far = TargetDistance(Models, Model);
			if (Model.Speed == 0)
			{
				Model.Speed = 1;
			}
			const uint16 Word = ReadU16(Models, Run.Word(1));
			Run.Next = Run.At + 4;
			const FGearBone* Bone = BoneAt(Models, Model, Word & 0xFF);
			if (Bone == nullptr)
			{
				return;
			}
			const int16 Channel = Bone->Rotation != FGearBone::NoChannel ? Bone->Rotation : Bone->Translation;
			const bool bAtTick = Channel != FGearBone::NoChannel && Models.Channels[Channel].Tick == static_cast<int32>(Word >> 8);
			const int32 Steps = Divide(Models, Far, Model.Speed);
			if (!bAtTick || (bNear ? static_cast<int32>(Run.B) <= Steps : Steps < static_cast<int32>(Run.B)))
			{
				Run.Stop();
			}
		}

		template <bool bNear>
		void WaitDistance(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			const int32 Far = TargetDistance(Models, Model);
			if (bNear ? Model.Speed <= Far : Far <= Model.Speed)
			{
				Run.Stop();
			}
		}

		FGearCodeRef Handler(FGearModels& Models, const FGearScriptRun& Run, int32 OffsetWord)
		{
			return Run.B == 0 ? FGearCodeRef{} : Run.At + ReadS16(Models, Run.Word(OffsetWord));
		}

		void OnNear(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			Model.NearDistance = Model.Speed;
			Model.OnNear = Handler(Models, Run, 1);
			Run.Next = Run.At + 4;
		}

		void LoopStart(FGearModels& Models, FGearScriptRun& Run, FGearModel&)
		{
			WriteU16(Models, Run.Word(1), 0);
			Run.Next = Run.At + 4;
		}

		void LoopEnd(FGearModels& Models, FGearScriptRun& Run, FGearModel&)
		{
			const FGearCodeRef Start = Run.At + ReadS16(Models, Run.Word(1));
			const int32 Count = ReadU16(Models, Start) >> 8;
			const uint16 Done = static_cast<uint16>(ReadU16(Models, Start + 2) + 1);
			WriteU16(Models, Start + 2, Done);
			Run.Next = static_cast<int16>(Done) < Count ? Start + 4 : Run.At + 4;
		}

		void Jump(FGearModels& Models, FGearScriptRun& Run, FGearModel&)
		{
			Run.Next = Run.At + ReadS16(Models, Run.Word(1));
		}

		void SkipWord(FGearModels&, FGearScriptRun& Run, FGearModel&)
		{
			Run.Next = Run.At + 4;
		}

		void JumpRandom(FGearModels& Models, FGearScriptRun& Run, FGearModel&)
		{
			const int16 Offset = ReadS16(Models, Run.Word(1));
			if (Models.Random == nullptr)
			{
				Models.Fail("the gears' module has no random numbers to draw");
				return;
			}
			Run.Next = Models.Random->Next() > HalfOfRand ? Run.At + Offset : Run.At + 4;
		}

		void OnTimer(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			Model.Timer = 0;
			Model.TimerEnd = ReadU16(Models, Run.Word(1));
			Model.OnTimer = Handler(Models, Run, 2);
			Run.Next = Run.At + 6;
		}

		void OnLanding(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			Model.OnLanding = Handler(Models, Run, 1);
			Run.Next = Run.At + 4;
		}

		/** The queue off and each held start made on the running model; the run ends when there were any. */
		void Flush(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model, uint8 Held)
		{
			Model.Queue = 0;
			if (Held <= 1)
			{
				return;
			}
			for (uint8 Index = 1; Index < Held && !Models.Faulted(); ++Index)
			{
				// Each start reads the queue as the starts before it left it.
				const FGearModel* Running = Models.Model(Run.Slot);
				if (Running == nullptr)
				{
					Models.Fail("a gear's model is freed while its held starts are made");
					break;
				}
				StartScript(Models, Run.Slot, Running->QueuedOwners[Index - 1u], Running->QueuedAnimations[Index - 1u]);
			}
			Run.bEnd = true;
		}

		void FlushIfQueued(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			const uint8 Held = Model.Queue;
			for (uint8 Index = 1; Index < Held; ++Index)
			{
				if (Model.QueuedAnimations[Index - 1u] == Run.B)
				{
					Flush(Models, Run, Model, Held);
					return;
				}
			}
		}

		void Queue(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			const uint8 Held = Model.Queue;
			if (Run.B == QueueOn && Held >= 2)
			{
				return;
			}
			Model.Queue = Run.B;
			if (Run.B == QueueFlush)
			{
				Flush(Models, Run, Model, Held);
			}
		}

		void JumpAtTarget(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			const int16 Offset = ReadS16(Models, Run.Word(1));
			const FGearBone& Root = Model.Bones[0];
			const bool bThere = Model.Target[0] == Root.Position[0] && Model.Target[1] == Root.Position[1] && Model.Target[2] == Root.Position[2];
			Run.Next = bThere ? Run.At + Offset : Run.At + 4;
		}

		/** The port reads files at once: none is ever pending. */
		void WaitFiles(FGearModels&, FGearScriptRun&, FGearModel&)
		{
		}

		void SetSignal(FGearModels&, FGearScriptRun& Run, FGearModel& Model)
		{
			Model.Signal = Run.B & 1;
		}

		void WaitSignal(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			uint16 Mask = 0;
			const uint32 Slot = SelectModel(Models, Model, Run.B, Mask) & 0xFF;
			const uint16 Word = ReadU16(Models, Run.Word(1));
			Run.Next = Run.At + 4;
			if (Slot >= FGearModels::Slots)
			{
				Models.Fail("a gear's script waits on slot " + std::to_string(Slot) + ", past the models' slots");
				return;
			}
			const FGearModel* Other = Models.Model(static_cast<int32>(Slot));
			if (Other != nullptr && Other->Signal == (Word & 1))
			{
				Run.Stop();
			}
		}

		void MarkMask(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			Model.Mark = Run.B == 0 ? int16{-1} : static_cast<int16>(Models.PlayMask);
		}

		void JumpIfMask(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			const int16 Offset = ReadS16(Models, Run.Word(1));
			Run.Next = Run.At + 4;
			if (static_cast<uint32>(static_cast<int32>(Model.Mark)) == Models.PlayMask)
			{
				Run.Stop(Run.At + Offset);
			}
		}
	}

	void AddFlowOps(std::array<FGearOp, 0x71>& Ops)
	{
		Ops[0x01] = &Wait;
		Ops[0x02] = &Notify;
		Ops[0x03] = &Notify;
		Ops[0x14] = &StartScripts;
		Ops[0x15] = &Clone;
		Ops[0x16] = &Clone;
		Ops[0x17] = &FreeSelf;
		Ops[0x1F] = &SwitchModel;
		Ops[0x20] = &WaitChannels;
		Ops[0x21] = &WaitTag;
		Ops[0x22] = &WaitLoops;
		Ops[0x28] = &WaitFrame<true>;
		Ops[0x29] = &WaitFrame<false>;
		Ops[0x2A] = &WaitDistance<true>;
		Ops[0x2B] = &WaitDistance<false>;
		Ops[0x2E] = &OnNear;
		Ops[0x30] = &LoopStart;
		Ops[0x31] = &LoopEnd;
		Ops[0x32] = &Jump;
		Ops[0x33] = &SkipWord;
		Ops[0x34] = &SkipWord;
		Ops[0x3B] = &SkipWord;
		Ops[0x35] = &JumpRandom;
		Ops[0x36] = &OnTimer;
		Ops[0x37] = &OnLanding;
		Ops[0x3D] = &FlushIfQueued;
		Ops[0x5B] = &Queue;
		Ops[0x5C] = &JumpAtTarget;
		Ops[0x6C] = &WaitFiles;
		Ops[0x6D] = &SetSignal;
		Ops[0x6E] = &WaitSignal;
		Ops[0x6F] = &MarkMask;
		Ops[0x70] = &JumpIfMask;
	}
}
