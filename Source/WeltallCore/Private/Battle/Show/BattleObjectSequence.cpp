#include "BattleObjectSequence.h"

#include "Battle/Show/BattleShowFloor.h"
#include "Graphics/GteVectors.h"

#include <string>

namespace Weltall::BattleObjectMachine
{
	namespace
	{
		/** A run this long never waits: the original spins in it for good (an unused sequence jumping to itself). */
		constexpr int32 EndlessRun = 0x10000;

		std::array<FSequenceOp, SequenceOpCount> MakeOps()
		{
			std::array<FSequenceOp, SequenceOpCount> Ops{};
			AddSequenceFlowOps(Ops);
			AddSequenceMotionOps(Ops);
			AddSequenceModelOps(Ops);
			AddSequenceWorldOps(Ops);
			return Ops;
		}

		const std::array<FSequenceOp, SequenceOpCount>& SequenceOps()
		{
			static const std::array<FSequenceOp, SequenceOpCount> Ops = MakeOps();
			return Ops;
		}

		/** The root's spin and velocity for the step: its angles turned by the spin, its position moved in its own frame. */
		void Move(FBattleObject& Object)
		{
			FGearBone& Root = Object.Bones[0];
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Object.Spin[Axis] = static_cast<int16>(Object.Spin[Axis] + Object.SpinAcceleration[Axis]);
				Object.Velocity[Axis] = static_cast<int16>(Object.Velocity[Axis] + Object.Acceleration[Axis]);
			}
			FGteVector Step{};
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Root.Angles[Axis] = static_cast<int16>(Root.Angles[Axis] + (Object.Spin[Axis] >> 3));
				Step[Axis] = static_cast<int16>((Object.Velocity[Axis] * Root.Scale[Axis]) >> 12);
			}
			const FGteLongVector Moved = ApplyMatrix(Root.World, Step);
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Root.Position[Axis] = GearMachine::WrapAdd(Root.Position[Axis], GearMachine::WrapMul(Object.Scale, Moved[Axis]) >> 12);
			}
		}

		/** The first handler due, if any: near the target, landed on the floor below, or the timer run out. */
		FGearCodeRef TakeHandler(FBattleObjects& Objects, FBattleObject& Object, int32 Ticks)
		{
			if (Object.OnNear && TargetDistance(Objects, Object) <= Object.NearDistance)
			{
				const FGearCodeRef Handler = Object.OnNear;
				Object.OnNear = {};
				return Handler;
			}
			FGearBone& Root = Object.Bones[0];
			if (Object.OnLanding)
			{
				FGteVector Ground = {static_cast<int16>(Root.Position[0]), 0, static_cast<int16>(Root.Position[2])};
				const std::optional<int32> Found = Objects.Floor->FindTriangle(Ground, Object.FloorTriangle, 4);
				Object.FloorTriangle = static_cast<int16>(Found.value_or(-1));
				if (Object.FloorTriangle < 0)
				{
					Object.FloorTriangle = static_cast<int16>(Objects.Floor->ScanTriangles(Ground));
				}
				Objects.Floor->Height(Ground, Object.FloorTriangle);
				if (Ground[1] < Root.Position[1])
				{
					Root.Position[1] = Ground[1];
					const FGearCodeRef Handler = Object.OnLanding;
					Object.OnLanding = {};
					return Handler;
				}
			}
			if (Object.OnTimer)
			{
				Object.Timer = static_cast<uint16>(Object.Timer + Ticks);
				if (Object.TimerEnd <= Object.Timer)
				{
					const FGearCodeRef Handler = Object.OnTimer;
					Object.OnTimer = {};
					return Handler;
				}
			}
			return Object.Pc;
		}
	}

	uint16 Word(FBattleObjects& Objects, const FSequenceRun& Run, int32 Index)
	{
		return GearMachine::ReadU16(Objects, Run.Word(Index));
	}

	int16 SignedWord(FBattleObjects& Objects, const FSequenceRun& Run, int32 Index)
	{
		return GearMachine::ReadS16(Objects, Run.Word(Index));
	}

	FGearCodeRef JumpTarget(FBattleObjects& Objects, const FSequenceRun& Run, int32 Index)
	{
		return Run.At + SignedWord(Objects, Run, Index);
	}

	FBattleObject* NamedObject(FBattleObjects& Objects, uint32 Slot)
	{
		if (Slot >= static_cast<uint32>(FBattleObjects::SlotCount))
		{
			Objects.Fail("a sequence names slot " + std::to_string(Slot) + ", past the objects' slots");
			return nullptr;
		}
		return Objects.Object(static_cast<int32>(Slot));
	}

	void FlushQueue(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object, uint8 Queued)
	{
		Object.Queue = 0;
		if (Queued <= 1)
		{
			return;
		}
		const int32 Slot = Objects.PlaceOf(Object);
		for (uint8 Index = 1; Index < Queued && !Objects.Faulted(); ++Index)
		{
			// Each start reads the queue as the starts before it left it.
			const FBattleObject* Running = Objects.Object(Slot);
			if (Running == nullptr)
			{
				Objects.Fail("an object is freed while its queued sequences start");
				break;
			}
			Objects.StartSequence(Slot, Running->QueuedOwners[Index - 1u], Running->QueuedSequences[Index - 1u]);
		}
		Run.bEnd = true;
	}

	void RunSequence(FBattleObjects& Objects, int32 Slot, int32 Events, int32 Ticks, int32 CatchUp)
	{
		FBattleObject* Object = Objects.Object(Slot);
		if (Object == nullptr || Ticks == 0 || !Object->Pc || Objects.Faulted())
		{
			return;
		}
		for (int32 Step = 0; Step < CatchUp + 1; ++Step)
		{
			Move(*Object);
		}
		FSequenceRun Run;
		Run.FirstSlot = Slot;
		Run.Slot = Slot;
		Run.At = TakeHandler(Objects, *Object, Ticks);
		Run.Events = Events;
		Run.Ticks = Ticks;
		if (Object->Who != 0)
		{
			UpdateTargetPoint(Objects, *Object);
		}
		const std::array<FSequenceOp, SequenceOpCount>& Ops = SequenceOps();
		for (int32 Instructions = 0; !Objects.Faulted(); ++Instructions)
		{
			if (Instructions == EndlessRun)
			{
				Objects.Fail("object " + std::to_string(Run.Slot) + "'s sequence runs without ever waiting, where the original hangs");
				return;
			}
			FBattleObject* Running = Objects.Object(Run.Slot);
			if (Running == nullptr)
			{
				Objects.Fail("an object is freed while its sequence runs");
				return;
			}
			const uint16 Instruction = GearMachine::ReadU16(Objects, Run.At);
			const uint8 Opcode = static_cast<uint8>(Instruction);
			Run.B = static_cast<uint8>(Instruction >> 8);
			Run.Next = Run.At + 2;
			const FSequenceOp Op = Opcode < Ops.size() ? Ops[Opcode] : nullptr;
			if (Op == nullptr)
			{
				Run.Stop();
			}
			else
			{
				Op(Objects, Run, *Running);
			}
			if (Run.bEnd || Objects.Faulted())
			{
				return;
			}
			if (Run.bStop)
			{
				if (FBattleObject* Keeper = Objects.Object(Run.Slot))
				{
					Keeper->Pc = Run.StopAt;
				}
				if (Run.bCountRecord)
				{
					Objects.Services->CountModelRecord();
				}
				return;
			}
			Run.At = Run.Next;
		}
	}
}
