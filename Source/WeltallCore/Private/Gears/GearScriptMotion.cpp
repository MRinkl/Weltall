#include "GearScript.h"

#include "Graphics/GteVectors.h"

#include <string>

namespace Weltall::GearMachine
{
	namespace
	{
		constexpr uint8 PlaceFromTargetMode = 0xFB;
		constexpr uint8 AttachFromPosition = 1;
		constexpr uint8 AttachTurns = 2;

		std::array<int16, 3> ReadTriple(FGearModels& Models, const FGearScriptRun& Run)
		{
			return {ReadS16(Models, Run.Word(1)), ReadS16(Models, Run.Word(2)), ReadS16(Models, Run.Word(3))};
		}

		/** The model's scale times its root's z scale (4.12), which the speeds are measured in. */
		int32 SpeedScale(const FGearModel& Model)
		{
			return (Model.Scale * Model.Bones[0].Scale[2]) >> 12;
		}

		void StopMotion(FGearModels&, FGearScriptRun&, FGearModel& Model)
		{
			Model.Spin = {};
			Model.SpinAcceleration = {};
			Model.Velocity = {};
			Model.Acceleration = {};
		}

		template <uint8 YawOnly>
		void TurnToTarget(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			const uint16 Word = ReadU16(Models, Run.Word(1));
			TurnRootToward(Models, Model, YawOnly, Run.B, static_cast<int16>(Word & 0xFF), static_cast<int16>(Word >> 8));
			Run.Next = Run.At + 4;
		}

		template <bool bRelative>
		void RotateRoot(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			std::array<int16, 3> Angles = ReadTriple(Models, Run);
			Run.Events = -1;
			if (bRelative)
			{
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Angles[Axis] = static_cast<int16>(Angles[Axis] + Model.Bones[0].Angles[Axis]);
				}
			}
			RotateRootTo(Models, Model, Run.B, Angles[0], Angles[1], Angles[2]);
			Run.Next = Run.At + 8;
		}

		template <bool bYawOnly>
		void FaceTarget(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			const FGearBone& Root = Model.Bones[0];
			const int32 X = Model.Target[0] - Root.Position[0];
			const int32 Y = bYawOnly ? 0 : Model.Target[1] - Root.Position[1];
			const int32 Z = Model.Target[2] - Root.Position[2];
			const int16 Pitch = bYawOnly ? int16{0} : static_cast<int16>(Ratan2(Models, Y, Distance(Models, X, 0, Z)));
			const int16 Yaw = static_cast<int16>(Ratan2(Models, -X, -Z));
			if (X != 0 || Y != 0 || Z != 0)
			{
				Run.Events = -1;
				RotateRootTo(Models, Model, Run.B, Pitch, Yaw, 0);
			}
		}

		/** 44-47, 4B-4E: set or add three words to one of the root's motion vectors. */
		template <std::array<int16, 3> FGearModel::*Vector, bool bAdd>
		void SetVector(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			const std::array<int16, 3> Values = ReadTriple(Models, Run);
			std::array<int16, 3>& Target = Model.*Vector;
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Target[Axis] = static_cast<int16>(Values[Axis] + (bAdd ? Target[Axis] : 0));
			}
			Run.Next = Run.At + 8;
		}

		void SetFreeHeight(FGearModels&, FGearScriptRun& Run, FGearModel& Model)
		{
			Model.FreeHeight = Run.B;
		}

		void SetPosition(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			const std::array<int16, 3> Values = ReadTriple(Models, Run);
			Model.Bones[0].Position = {Values[0], Values[1], Values[2]};
			Run.Next = Run.At + 8;
		}

		void PlaceFromTarget(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			if (Run.B != PlaceFromTargetMode)
			{
				return;
			}
			FGearBone& Root = Model.Bones[0];
			std::array<int32, 3> Away{};
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Away[Axis] = Root.Position[Axis] - Model.Target[Axis];
			}
			const int32 Far = Distance(Models, Away[0], Away[1], Away[2]) + 1;
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Root.Position[Axis] = Model.Target[Axis] + Divide(Models, WrapMul(Away[Axis], Model.Speed), Far);
			}
		}

		void VelocityToTarget(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			const FGearBone& Root = Model.Bones[0];
			const int32 X = Model.Target[0] - Root.Position[0];
			const int32 Y = Model.Target[1] - Root.Position[1];
			const int32 Z = Model.Target[2] - Root.Position[2];
			const int32 Ticks = Run.B == 0 ? 1 : Run.B;
			const int32 Far = Distance(Models, X, Y, Z);
			const int32 PerTick = WrapMul(Divide(Models, Far, Ticks), 0x1000);
			Model.Velocity[2] = static_cast<int16>(Divide(Models, PerTick, SpeedScale(Model)) / 2);
			const int32 Yaw = Ratan2(Models, -X, -Z);
			// Within a quarter turn of the root's yaw the velocity is negated.
			if (((static_cast<uint32>(Yaw) - static_cast<uint16>(Root.Angles[1]) + 0x400) & 0xFFF) < 0x800)
			{
				Model.Velocity[2] = static_cast<int16>(-Model.Velocity[2]);
			}
		}

		void SetTarget(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			Model.Target = ReadTriple(Models, Run);
			Run.Next = Run.At + 8;
		}

		void SetSpeed(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			Model.Speed = static_cast<int16>(ReadS16(Models, Run.Word(1)) * SpeedScale(Model) >> 12);
			Run.Next = Run.At + 4;
		}

		void AddSpeed(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			Model.Speed = static_cast<int16>(Model.Speed + static_cast<int16>(ReadS16(Models, Run.Word(1)) * SpeedScale(Model) >> 12));
			Run.Next = Run.At + 4;
		}

		void AddSpeedRaw(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			Model.Speed = static_cast<int16>(Model.Speed + ReadS16(Models, Run.Word(1)));
			Run.Next = Run.At + 4;
		}

		void AddRadius(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			uint16 Mask = 0;
			const uint32 Slot = SelectModel(Models, Model, Run.B, Mask) & 0xFF;
			if (Slot >= FGearModels::Slots)
			{
				Models.Fail("a gear's script measures slot " + std::to_string(Slot) + ", past the models' slots");
				return;
			}
			Model.Speed = static_cast<int16>(Model.Speed + static_cast<int16>(Radius(Models, static_cast<int32>(Slot))));
		}

		/** The bone's axes in the model's frame: its world matrix's columns. */
		FGteLongVector Column(const FGteMatrix& World, std::size_t Axis)
		{
			return {World.M[0][Axis], World.M[1][Axis], World.M[2][Axis]};
		}

		void Attach(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			const uint16 Word = ReadU16(Models, Run.Word(1));
			const std::array<int16, 3> Offset = {ReadS16(Models, Run.Word(2)), ReadS16(Models, Run.Word(3)), ReadS16(Models, Run.Word(4))};
			Run.Next = Run.At + 10;
			uint16 Mask = 0;
			SelectModel(Models, Model, Word & 0xFF, Mask);
			const int32 Bone = Word >> 8;
			for (int32 Slot = 0; Slot < 8 && !Models.Faulted(); ++Slot)
			{
				FGearModel* Attached = ((Mask >> Slot) & 1) != 0 ? Models.Model(Slot) : nullptr;
				if (Attached == nullptr)
				{
					continue;
				}
				Attached->HostBone = static_cast<uint16>(Bone);
				Attached->HostSlot = Model.Slot;
				Attached->HostTurns = Run.B & AttachTurns;
				Attached->FreeHeight = 1;
				if ((Run.B & AttachFromPosition) == 0)
				{
					Attached->AttachOffset = Offset;
					continue;
				}
				const FGearBone* Host = BoneAt(Models, Model, Bone);
				if (Host == nullptr)
				{
					return;
				}
				const FGteLongVector AxisX = Column(Host->World, 0);
				const FGteLongVector AxisY = Column(Host->World, 1);
				const FGteLongVector AxisZ = Column(Host->World, 2);
				const std::array<int32, 3> Away = {Attached->Bones[0].Position[0] - Host->World.T[0], Attached->Bones[0].Position[1] - Host->World.T[1],
					Attached->Bones[0].Position[2] - Host->World.T[2]};
				Attached->AttachOffset = {ProjectOnAxis(Models, Away, AxisY, AxisZ, Model.Scale), ProjectOnAxis(Models, Away, AxisZ, AxisX, Model.Scale),
					ProjectOnAxis(Models, Away, AxisX, AxisY, Model.Scale)};
			}
		}

		void Detach(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			uint16 Mask = 0;
			SelectModel(Models, Model, Run.B, Mask);
			for (int32 Slot = 0; Slot < 8; ++Slot)
			{
				if (((Mask >> Slot) & 1) != 0)
				{
					if (FGearModel* Attached = Models.Model(Slot))
					{
						Attached->HostSlot = FGearModel::NoHost;
					}
				}
			}
		}
	}

	void AddMotionOps(std::array<FGearOp, 0x71>& Ops)
	{
		Ops[0x0C] = &StopMotion;
		Ops[0x25] = &Attach;
		Ops[0x26] = &Detach;
		Ops[0x38] = &TurnToTarget<0>;
		Ops[0x39] = &TurnToTarget<1>;
		Ops[0x40] = &RotateRoot<false>;
		Ops[0x41] = &RotateRoot<true>;
		Ops[0x42] = &FaceTarget<false>;
		Ops[0x43] = &FaceTarget<true>;
		Ops[0x44] = &SetVector<&FGearModel::Spin, false>;
		Ops[0x45] = &SetVector<&FGearModel::Spin, true>;
		Ops[0x46] = &SetVector<&FGearModel::SpinAcceleration, false>;
		Ops[0x47] = &SetVector<&FGearModel::SpinAcceleration, true>;
		Ops[0x48] = &SetFreeHeight;
		Ops[0x49] = &SetPosition;
		Ops[0x4A] = &PlaceFromTarget;
		Ops[0x4B] = &SetVector<&FGearModel::Velocity, false>;
		Ops[0x4C] = &SetVector<&FGearModel::Velocity, true>;
		Ops[0x4D] = &SetVector<&FGearModel::Acceleration, false>;
		Ops[0x4E] = &SetVector<&FGearModel::Acceleration, true>;
		Ops[0x4F] = &VelocityToTarget;
		Ops[0x50] = &SetTarget;
		Ops[0x54] = &SetSpeed;
		Ops[0x55] = &AddSpeed;
		Ops[0x56] = &AddSpeedRaw;
		Ops[0x57] = &AddRadius;
	}
}
