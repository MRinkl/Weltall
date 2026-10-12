#include "GearScript.h"

#include "Graphics/GteVectors.h"

#include <string>

namespace Weltall::GearMachine
{
	namespace
	{
		/** Scripts from 50h would be in a second table, which no model has. */
		constexpr int32 FirstSecondTableScript = 0x50;
		constexpr uint8 QueueFull = 5;
		/** Opcodes the handler table passes over as two-byte instructions that do nothing. */
		constexpr uint8 NoOps[] = {0x04, 0x05, 0x06, 0x07, 0x09, 0x0F, 0x12, 0x1B, 0x1C, 0x2C, 0x2D, 0x2F, 0x3A, 0x3E, 0x3F,
			0x51, 0x52, 0x53, 0x58, 0x59, 0x5A, 0x60, 0x61, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A};

		void Nothing(FGearModels&, FGearScriptRun&, FGearModel&)
		{
		}

		std::array<FGearOp, 0x71> MakeOps()
		{
			std::array<FGearOp, 0x71> Ops{};
			for (const uint8 Opcode : NoOps)
			{
				Ops[Opcode] = &Nothing;
			}
			AddChannelOps(Ops);
			AddFlowOps(Ops);
			AddMotionOps(Ops);
			AddModelOps(Ops);
			return Ops;
		}

		/** The root's spin and velocity for each tick: its angles turned by the spin, its position moved in its own frame. */
		void Move(FGearModel& Model, int32 Ticks)
		{
			FGearBone& Root = Model.Bones[0];
			for (int32 Tick = 0; Tick < Ticks; ++Tick)
			{
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Model.Spin[Axis] = static_cast<int16>(Model.Spin[Axis] + Model.SpinAcceleration[Axis]);
					Model.Velocity[Axis] = static_cast<int16>(Model.Velocity[Axis] + Model.Acceleration[Axis]);
				}
				FGteVector Step{};
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Root.Angles[Axis] = static_cast<int16>(Root.Angles[Axis] + (Model.Spin[Axis] >> 3));
				}
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Step[Axis] = static_cast<int16>((Model.Velocity[Axis] * Root.Scale[Axis]) >> 12);
				}
				const FGteLongVector Moved = ApplyMatrix(Root.World, Step);
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Root.Position[Axis] = WrapAdd(Root.Position[Axis], WrapMul(Model.Scale, Moved[Axis]) >> 12);
				}
			}
		}

		/** The first handler due, if any: near the target, landed on the floor, or the timer run out. */
		FGearCodeRef TakeHandler(FGearModels& Models, FGearModel& Model, int32 Ticks)
		{
			if (Model.OnNear && TargetDistance(Models, Model) <= Model.NearDistance)
			{
				const FGearCodeRef Handler = Model.OnNear;
				Model.OnNear = {};
				return Handler;
			}
			FGearBone& Root = Model.Bones[0];
			if (Model.OnLanding && Model.FloorY < Root.Position[1])
			{
				Root.Position[1] = Model.FloorY;
				const FGearCodeRef Handler = Model.OnLanding;
				Model.OnLanding = {};
				return Handler;
			}
			if (Model.OnTimer)
			{
				Model.Timer = static_cast<uint16>(Model.Timer + Ticks);
				if (Model.TimerEnd <= Model.Timer)
				{
					const FGearCodeRef Handler = Model.OnTimer;
					Model.OnTimer = {};
					return Handler;
				}
			}
			return Model.Pc;
		}
	}

	const std::array<FGearOp, 0x71>& GearOps()
	{
		static const std::array<FGearOp, 0x71> Ops = MakeOps();
		return Ops;
	}

	void StartScript(FGearModels& Models, int32 Slot, int32 OwnerSlot, int32 Animation)
	{
		FGearModel* Model = Models.Model(Slot);
		if (Model == nullptr)
		{
			return;
		}
		if (OwnerSlot < 0 || OwnerSlot >= FGearModels::Slots)
		{
			Models.Fail("a gear's script names owner " + std::to_string(OwnerSlot) + ", past the models' slots");
			return;
		}
		FGearModel* Owner = Models.Model(OwnerSlot);
		if (Owner == nullptr)
		{
			return;
		}
		if (Model->Queue != 0)
		{
			if (Model->Queue < QueueFull)
			{
				++Model->Queue;
			}
			Model->QueuedOwners[static_cast<std::size_t>(Model->Queue - 2)] = Owner->Slot;
			Model->QueuedAnimations[static_cast<std::size_t>(Model->Queue - 2)] = static_cast<uint8>(Animation);
			return;
		}
		const std::optional<uint32> Script = Animation < FirstSecondTableScript ? Owner->Motion->Script(Animation) : std::nullopt;
		if (!Script)
		{
			Models.Fail("a gear plays animation " + std::to_string(Animation) + ", past its scripts");
			return;
		}
		Model->Pc = {Owner->Motion.get(), *Script};
		Model->Loops = 0;
		Model->WaitTicks = 0;
		Model->OnTimer = {};
		Model->OnLanding = {};
		Model->OnNear = {};
		Model->PlayMask = Models.PlayMask;
		RunScript(Models, Slot, -1, 1);
	}

	void RunScript(FGearModels& Models, int32 Slot, int32 Events, int32 Ticks)
	{
		FGearModel* Model = Models.Model(Slot);
		if (Model == nullptr || Ticks == 0 || !Model->Pc)
		{
			return;
		}
		Move(*Model, Ticks);
		FGearScriptRun Run;
		Run.FirstSlot = Slot;
		Run.Slot = Slot;
		Run.At = TakeHandler(Models, *Model, Ticks);
		Run.Events = Events;
		Run.Ticks = Ticks;
		const std::array<FGearOp, 0x71>& Ops = GearOps();
		while (!Models.Faulted())
		{
			FGearModel* Running = Models.Model(Run.Slot);
			if (Running == nullptr)
			{
				Models.Fail("a gear's model is freed while its script runs");
				return;
			}
			const uint16 Word = ReadU16(Models, Run.At);
			const uint8 Opcode = static_cast<uint8>(Word);
			Run.B = static_cast<uint8>(Word >> 8);
			Run.Next = Run.At + 2;
			const FGearOp Op = Opcode < Ops.size() ? Ops[Opcode] : nullptr;
			if (Op == nullptr)
			{
				Run.Stop();
			}
			else
			{
				Op(Models, Run, *Running);
			}
			if (Run.bEnd || Models.Faulted())
			{
				return;
			}
			if (Run.bStop)
			{
				if (FGearModel* Keeper = Models.Model(Run.Slot))
				{
					Keeper->Pc = Run.StopAt;
				}
				return;
			}
			Run.At = Run.Next;
		}
	}
}
