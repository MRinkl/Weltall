#include "Battle/Show/BattleGearOpening.h"

#include "Foundation/LittleEndian.h"
#include "Gears/GearFiles.h"
#include "Gears/GearSkeleton.h"

namespace Weltall
{
	namespace
	{
		// The sequence instructions a gear's first sequence sets its model up with, each a word (the opcode, then its
		// operand) and the words that follow it.
		constexpr uint8 StopBoneTracks = 0x0A;
		constexpr uint8 StopVelocities = 0x0C;
		constexpr uint8 SetPose = 0x10;
		constexpr uint8 ShowBone = 0x23;
		constexpr uint8 SetFloorSnap = 0x48;
		constexpr uint8 Place = 0x4A;
		constexpr uint8 SetBoneDrawKind = 0x5D;
		constexpr uint8 SetWind = 0x64;
		/** The branch on its side's room, which ends the set-up. */
		constexpr uint8 BranchOnRoom = 0x61;
		constexpr uint8 ShownBit = 0x01;
		constexpr uint8 WithChildrenBit = 0x80;
		constexpr uint32 MostInstructions = 64;

		/** The bytes an instruction of the set-up takes, 0 for any other. */
		uint32 SetUpBytes(uint8 Opcode)
		{
			switch (Opcode)
			{
			case StopBoneTracks:
			case StopVelocities:
			case SetPose:
			case SetFloorSnap:
			case Place:
				return 2;
			case ShowBone:
			case SetBoneDrawKind:
			case SetWind:
				return 4;
			default:
				return 0;
			}
		}

		void Show(std::span<FGearBone> Bones, int32 Bone, bool bShown, bool bWithChildren)
		{
			if (Bone < 0 || static_cast<std::size_t>(Bone) >= Bones.size())
			{
				return;
			}
			Bones[static_cast<std::size_t>(Bone)].bShown = bShown ? 1 : 0;
			if (!bWithChildren)
			{
				return;
			}
			// A skeleton lists parents before their children.
			for (std::size_t Child = static_cast<std::size_t>(Bone) + 1; Child < Bones.size(); ++Child)
			{
				if (Bones[Child].Parent == Bone)
				{
					Show(Bones, static_cast<int32>(Child), bShown, true);
				}
			}
		}
	}

	FBattleGearOpening FBattleGearOpening::Read(const FGearMotionPack& Sequences)
	{
		FBattleGearOpening Opening;
		const std::optional<uint32> First = Sequences.Script(0);
		if (!First)
		{
			return Opening;
		}
		uint32 At = *First;
		for (uint32 Count = 0; Count < MostInstructions && Sequences.Has(At, 2); ++Count)
		{
			const uint8 Opcode = Sequences.Bytes[At];
			const uint8 Operand = Sequences.Bytes[At + 1];
			const uint32 Bytes = SetUpBytes(Opcode);
			if (Bytes == 0 || !Sequences.Has(At, Bytes))
			{
				Opening.bComplete = Opcode == BranchOnRoom;
				break;
			}
			if (Opcode == ShowBone)
			{
				Opening.Shown.push_back({static_cast<int16>(LittleEndian::ReadU16(Sequences.Bytes, At + 2)), (Operand & ShownBit) != 0, (Operand & WithChildrenBit) != 0});
			}
			else if (Opcode == SetBoneDrawKind)
			{
				Opening.Billboards.push_back({static_cast<int16>(LittleEndian::ReadU16(Sequences.Bytes, At + 2)), static_cast<int16>(Operand)});
			}
			At += Bytes;
		}
		return Opening;
	}

	void FBattleGearOpening::Apply(std::span<FGearBone> Bones) const
	{
		for (const FShownBone& Bone : Shown)
		{
			Show(Bones, Bone.Bone, Bone.bShown, Bone.bWithChildren);
		}
		for (const FBillboard& Billboard : Billboards)
		{
			if (Billboard.Bone >= 0 && static_cast<std::size_t>(Billboard.Bone) < Bones.size())
			{
				Bones[static_cast<std::size_t>(Billboard.Bone)].Billboard = Billboard.Mode;
			}
		}
	}
}
