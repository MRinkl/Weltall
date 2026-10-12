#include "BattleObjectSequence.h"

#include "Battle/Show/BattleShowFloor.h"
#include "Battle/Show/BattleStageFiles.h"
#include "Graphics/GteVectors.h"

#include <string>

namespace Weltall::BattleObjectMachine
{
	namespace
	{
		constexpr uint8 PlaceFromTarget = 0xFB;
		constexpr uint8 AttachFromPosition = 1;
		constexpr uint8 AttachTurns = 2;
		constexpr int32 MaskedSlots = 13;
		/** The stage record's gear places: two corners (x, z) a group, from 100h. */
		constexpr uint32 GearPlaces = 0x100;
		constexpr uint32 GearPlaceBytes = 8;

		std::array<int16, 3> ReadTriple(FBattleObjects& Objects, const FSequenceRun& Run)
		{
			return {SignedWord(Objects, Run, 1), SignedWord(Objects, Run, 2), SignedWord(Objects, Run, 3)};
		}

		/** The object's scale times its root's z scale (4.12), which the speeds are measured in. */
		int32 SpeedScale(const FBattleObject& Object)
		{
			return (Object.Scale * Object.Bones[0].Scale[2]) >> 12;
		}

		void StopMotion(FBattleObjects&, FSequenceRun&, FBattleObject& Object)
		{
			Object.Spin = {};
			Object.SpinAcceleration = {};
			Object.Velocity = {};
			Object.Acceleration = {};
		}

		/** The bone's axes in the object's frame: its world matrix's columns. */
		FGteLongVector Column(const FGteMatrix& World, std::size_t Axis)
		{
			return {World.M[0][Axis], World.M[1][Axis], World.M[2][Axis]};
		}

		void Attach(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const uint16 Operand = Word(Objects, Run, 1);
			const std::array<int16, 3> Offset = {SignedWord(Objects, Run, 2), SignedWord(Objects, Run, 3), SignedWord(Objects, Run, 4)};
			Run.Next = Run.At + 10;
			uint16 Mask = 0;
			ResolveWho(Objects, Object, Operand & 0xFF, Mask);
			const int32 Bone = Operand >> 8;
			for (int32 Slot = 0; Slot < MaskedSlots && !Objects.Faulted(); ++Slot)
			{
				FBattleObject* Attached = ((static_cast<int16>(Mask) >> Slot) & 1) != 0 ? Objects.Object(Slot) : nullptr;
				if (Attached == nullptr)
				{
					continue;
				}
				Attached->HostBone = static_cast<uint16>(Bone);
				Attached->Host = Object.Slot;
				Attached->HostTurns = Run.B & AttachTurns;
				Attached->NoFloorSnap = 1;
				if ((Run.B & AttachFromPosition) == 0)
				{
					Attached->AttachOffset = Offset;
					continue;
				}
				const FGearBone* Host = GearMachine::BoneAt(Objects, Object, Bone);
				if (Host == nullptr)
				{
					return;
				}
				const FGteLongVector AxisX = Column(Host->World, 0);
				const FGteLongVector AxisY = Column(Host->World, 1);
				const FGteLongVector AxisZ = Column(Host->World, 2);
				const std::array<int32, 3> Away = {Attached->Bones[0].Position[0] - Host->World.T[0], Attached->Bones[0].Position[1] - Host->World.T[1],
					Attached->Bones[0].Position[2] - Host->World.T[2]};
				Attached->AttachOffset = {GearMachine::ProjectOnAxis(Objects, Away, AxisY, AxisZ, Object.Scale),
					GearMachine::ProjectOnAxis(Objects, Away, AxisZ, AxisX, Object.Scale), GearMachine::ProjectOnAxis(Objects, Away, AxisX, AxisY, Object.Scale)};
			}
		}

		void Detach(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			uint16 Mask = 0;
			ResolveWho(Objects, Object, Run.B, Mask);
			for (int32 Slot = 0; Slot < MaskedSlots; ++Slot)
			{
				if (((static_cast<int16>(Mask) >> Slot) & 1) != 0)
				{
					if (FBattleObject* Attached = Objects.Object(Slot))
					{
						Attached->Host = FBattleObject::NoHost;
					}
				}
			}
		}

		template <uint8 YawOnly>
		void TurnToTarget(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const uint16 Operand = Word(Objects, Run, 1);
			GearMachine::TurnRootToward(Objects, Object, YawOnly, Run.B, static_cast<int16>(Operand & 0xFF), static_cast<int16>(Operand >> 8));
			Run.Next = Run.At + 4;
		}

		template <bool bRelative>
		void RotateRoot(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			std::array<int16, 3> Angles = ReadTriple(Objects, Run);
			Run.Events = -1;
			if (bRelative)
			{
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Angles[Axis] = static_cast<int16>(Angles[Axis] + Object.Bones[0].Angles[Axis]);
				}
			}
			GearMachine::RotateRootTo(Objects, Object, Run.B, Angles[0], Angles[1], Angles[2]);
			Run.Next = Run.At + 8;
		}

		template <bool bYawOnly>
		void FaceTarget(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const FGearBone& Root = Object.Bones[0];
			const int32 X = Object.Target[0] - Root.Position[0];
			const int32 Y = bYawOnly ? 0 : Object.Target[1] - Root.Position[1];
			const int32 Z = Object.Target[2] - Root.Position[2];
			const int16 Pitch = bYawOnly ? int16{0} : static_cast<int16>(GearMachine::Ratan2(Objects, Y, GearMachine::Distance(Objects, X, 0, Z)));
			const int16 Yaw = static_cast<int16>(GearMachine::Ratan2(Objects, -X, -Z));
			if (X != 0 || Y != 0 || Z != 0)
			{
				Run.Events = -1;
				GearMachine::RotateRootTo(Objects, Object, Run.B, Pitch, Yaw, 0);
			}
		}

		/** 44-47, 4B-4E: set or add three words to one of the root's motion vectors. */
		template <std::array<int16, 3> FBattleObject::*Vector, bool bAdd>
		void SetVector(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const std::array<int16, 3> Values = ReadTriple(Objects, Run);
			std::array<int16, 3>& Target = Object.*Vector;
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Target[Axis] = static_cast<int16>(Values[Axis] + (bAdd ? Target[Axis] : 0));
			}
			Run.Next = Run.At + 8;
		}

		void SetNoFloorSnap(FBattleObjects&, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.NoFloorSnap = Run.B;
		}

		void SetPosition(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const std::array<int16, 3> Values = ReadTriple(Objects, Run);
			Object.Bones[0].Position = {Values[0], Values[1], Values[2]};
			Run.Next = Run.At + 8;
		}

		/** 4A: FBh a distance of the approach distance from the target point, along the way it lies; else at a slot's place. */
		void Place(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			FGearBone& Root = Object.Bones[0];
			if (Run.B != PlaceFromTarget)
			{
				uint16 Mask = 0;
				const uint32 Slot = ResolveWho(Objects, Object, Run.B, Mask) & 0xFF;
				if (const std::optional<std::array<int16, 3>> At = SlotPlace(Objects, static_cast<int32>(Slot)))
				{
					// The record's words go in unsigned.
					Root.Position[0] = static_cast<uint16>((*At)[0]);
					Root.Position[2] = static_cast<uint16>((*At)[2]);
				}
				return;
			}
			std::array<int32, 3> Away{};
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Away[Axis] = Root.Position[Axis] - Object.Target[Axis];
			}
			const int32 Far = GearMachine::Distance(Objects, Away[0], Away[1], Away[2]) + 1;
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Root.Position[Axis] = Object.Target[Axis] + GearMachine::Divide(Objects, GearMachine::WrapMul(Away[Axis], Object.Speed), Far);
			}
		}

		void VelocityToTarget(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const FGearBone& Root = Object.Bones[0];
			const int32 X = Object.Target[0] - Root.Position[0];
			const int32 Y = Object.Target[1] - Root.Position[1];
			const int32 Z = Object.Target[2] - Root.Position[2];
			const int32 Ticks = Run.B == 0 ? 1 : Run.B;
			const int32 Far = GearMachine::Distance(Objects, X, Y, Z);
			const int32 PerTick = GearMachine::WrapMul(GearMachine::Divide(Objects, Far, Ticks), 0x1000);
			Object.Velocity[2] = static_cast<int16>(GearMachine::Divide(Objects, PerTick, SpeedScale(Object)) / 2);
			const int32 Yaw = GearMachine::Ratan2(Objects, -X, -Z);
			// Within a quarter turn of the root's yaw the velocity is negated.
			if (((static_cast<uint32>(Yaw) - static_cast<uint16>(Root.Angles[1]) + 0x400) & 0xFFF) < 0x800)
			{
				Object.Velocity[2] = static_cast<int16>(-Object.Velocity[2]);
			}
		}

		void SetTarget(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.Who = 0;
			Object.Target = ReadTriple(Objects, Run);
			Run.Next = Run.At + 8;
		}

		void TargetSlot(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.Who = 0;
			uint16 Mask = 0;
			const uint32 Slot = ResolveWho(Objects, Object, Run.B, Mask) & 0xFF;
			if (const std::optional<std::array<int16, 3>> At = SlotPlace(Objects, static_cast<int32>(Slot)))
			{
				Object.Target = *At;
			}
		}

		void FollowBone(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.Who = Run.B;
			Object.WhoBone = SignedWord(Objects, Run, 1);
			Object.WhoOffset = {SignedWord(Objects, Run, 2), SignedWord(Objects, Run, 3), SignedWord(Objects, Run, 4)};
			Run.Next = Run.At + 10;
			if (Run.B != 0)
			{
				UpdateTargetPoint(Objects, Object);
			}
		}

		void Follow(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.Who = Run.B;
			Object.WhoBone = 0;
			Object.WhoOffset = {};
			if (Run.B != 0)
			{
				UpdateTargetPoint(Objects, Object);
			}
		}

		void TargetToGround(FBattleObjects& Objects, FSequenceRun&, FBattleObject& Object)
		{
			FGteVector Ground = {Object.Target[0], 0, Object.Target[2]};
			const int32 Triangle = Objects.Floor->ScanTriangles(Ground);
			Objects.Floor->Height(Ground, Triangle);
			Object.Target[1] = Ground[1];
		}

		void SetSpeed(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.Speed = static_cast<int16>(SignedWord(Objects, Run, 1) * SpeedScale(Object) >> 12);
			Run.Next = Run.At + 4;
		}

		void AddSpeed(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.Speed = static_cast<int16>(Object.Speed + static_cast<int16>(SignedWord(Objects, Run, 1) * SpeedScale(Object) >> 12));
			Run.Next = Run.At + 4;
		}

		void AddSpeedRaw(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.Speed = static_cast<int16>(Object.Speed + SignedWord(Objects, Run, 1));
			Run.Next = Run.At + 4;
		}

		void AddRadius(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			uint16 Mask = 0;
			const uint32 Slot = ResolveWho(Objects, Object, Run.B, Mask) & 0xFF;
			if (Slot >= static_cast<uint32>(FBattleObjects::SlotCount))
			{
				Objects.Fail("a sequence measures slot " + std::to_string(Slot) + ", past the objects' slots");
				return;
			}
			Object.Speed = static_cast<int16>(Object.Speed + static_cast<int16>(Objects.Radius(static_cast<int32>(Slot))));
		}

		template <bool bEye>
		void TargetPreset(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.Who = 0;
			const std::optional<FBattleCameraPreset> Preset = Objects.Stage->CameraPreset(Run.B);
			if (!Preset)
			{
				Objects.Fail("a sequence reads camera preset " + std::to_string(Run.B) + ", past the stage's record");
				return;
			}
			const FGteVector& Point = bEye ? Preset->Eye : Preset->Target;
			Object.Target = {Point[0], Point[1], Point[2]};
		}

		/** 60: the middle of the gear place of the object's slot's group. */
		void TargetGearPlace(FBattleObjects& Objects, FSequenceRun&, FBattleObject& Object)
		{
			const std::optional<uint8> Group = SlotGroup(Objects, Object.Slot);
			if (!Group)
			{
				return;
			}
			const std::span<const uint8> Record = Objects.Stage->Bytes();
			const uint32 At = GearPlaces + *Group * GearPlaceBytes;
			if (At + GearPlaceBytes > Record.size())
			{
				Objects.Fail("a sequence reads gear place " + std::to_string(*Group) + ", past the stage's record");
				return;
			}
			const auto Read = [&](uint32 Offset) { return static_cast<uint32>(Record[At + Offset] | Record[At + Offset + 1] << 8); };
			Object.Target = {static_cast<int16>((Read(0) + Read(4)) >> 1), 0, static_cast<int16>((Read(2) + Read(6)) >> 1)};
		}
	}

	void AddSequenceMotionOps(std::array<FSequenceOp, SequenceOpCount>& Ops)
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
		Ops[0x44] = &SetVector<&FBattleObject::Spin, false>;
		Ops[0x45] = &SetVector<&FBattleObject::Spin, true>;
		Ops[0x46] = &SetVector<&FBattleObject::SpinAcceleration, false>;
		Ops[0x47] = &SetVector<&FBattleObject::SpinAcceleration, true>;
		Ops[0x48] = &SetNoFloorSnap;
		Ops[0x49] = &SetPosition;
		Ops[0x4A] = &Place;
		Ops[0x4B] = &SetVector<&FBattleObject::Velocity, false>;
		Ops[0x4C] = &SetVector<&FBattleObject::Velocity, true>;
		Ops[0x4D] = &SetVector<&FBattleObject::Acceleration, false>;
		Ops[0x4E] = &SetVector<&FBattleObject::Acceleration, true>;
		Ops[0x4F] = &VelocityToTarget;
		Ops[0x50] = &SetTarget;
		Ops[0x51] = &TargetSlot;
		Ops[0x52] = &FollowBone;
		Ops[0x53] = &TargetToGround;
		Ops[0x54] = &SetSpeed;
		Ops[0x55] = &AddSpeed;
		Ops[0x56] = &AddSpeedRaw;
		Ops[0x57] = &AddRadius;
		Ops[0x58] = &Follow;
		Ops[0x59] = &TargetPreset<true>;
		Ops[0x5A] = &TargetPreset<false>;
		Ops[0x60] = &TargetGearPlace;
	}
}
