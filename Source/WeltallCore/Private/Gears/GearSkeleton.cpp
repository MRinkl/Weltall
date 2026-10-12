#include "Gears/GearSkeleton.h"

#include "Gears/GearFiles.h"
#include "Graphics/GteVectors.h"

namespace Weltall
{
	namespace
	{
		constexpr int16 FullScale = 0x1000;
		/** The root turns in libgte's y, x, z order; the other bones in the kernel's. */
		constexpr uint8 RootRotationOrder = 1;

		FGteMatrix Diagonal(int32 X, int32 Y, int32 Z)
		{
			FGteMatrix Result;
			Result.M[0][0] = static_cast<int16>(X);
			Result.M[1][1] = static_cast<int16>(Y);
			Result.M[2][2] = static_cast<int16>(Z);
			return Result;
		}

		/** The root's world matrix (its rotation, its position) and its local one (the rotation scaled). */
		void ComputeRoot(FGearBone& Root, int32 ModelScale)
		{
			Root.World.M = GearBoneRotation(Root.Angles, Root.RotationOrder).M;
			Root.World.T = Root.Position;
			const FGteMatrix Scale = Diagonal((ModelScale * Root.Scale[0]) >> 12, (ModelScale * Root.Scale[1]) >> 12, (ModelScale * Root.Scale[2]) >> 12);
			Root.Local.M = MulMatrix(Root.World, Scale).M;
			Root.Local.T = Root.World.T;
		}

		void Place(std::span<FGearBone> Bones, FGearBone& Bone)
		{
			if (Bone.Parent != FGearBone::NoParent && Bones[static_cast<std::size_t>(Bone.Parent)].bWorldDirty == 1)
			{
				Bone.bWorldDirty = 1;
			}
			if (Bone.bWorldDirty == 0)
			{
				return;
			}
			Bone.Local.T = Bone.Position;
			Bone.World = Bone.Parent == FGearBone::NoParent ? Bone.Local : CompMatrix(Bones[static_cast<std::size_t>(Bone.Parent)].World, Bone.Local);
		}
	}

	FGteMatrix GearBoneRotation(const FGteVector& Angles, uint8 RotationOrder)
	{
		return RotationOrder == 0 ? RotationXyz(Angles) : RotationYxz(Angles);
	}

	void MakeGearSkeleton(std::span<const FGearSkeletonEntry> Skeleton, std::vector<FGearBone>& Bones)
	{
		Bones.assign(Skeleton.size() + 1, FGearBone{});
		FGearBone& Root = Bones[0];
		Root.bWorldDirty = 1;
		Root.bRotationDirty = 1;
		Root.RotationOrder = RootRotationOrder;
		Root.bShown = 0;
		Root.Index = static_cast<uint16>(Bones.size());
		Root.Scale = {FullScale, FullScale, FullScale};
		for (std::size_t Entry = 0; Entry < Skeleton.size(); ++Entry)
		{
			FGearBone& Bone = Bones[Entry + 1];
			Bone.Parent = Skeleton[Entry].Parent == 0xFFFF ? FGearBone::NoParent : static_cast<int16>(Skeleton[Entry].Parent + 1);
			Bone.Index = static_cast<uint16>(Entry + 1);
			Bone.bWorldDirty = 1;
			Bone.bRotationDirty = 1;
			Bone.bShown = 1;
			Bone.Scale = {FullScale, FullScale, FullScale};
			Bone.Part = Skeleton[Entry].Part;
		}
	}

	void ComputeGearBones(std::span<FGearBone> Bones, int32 ModelScale)
	{
		FGearBone& Root = Bones[0];
		ComputeRoot(Root, ModelScale);
		const std::size_t Count = Root.Index;
		for (std::size_t Index = 1; Index < Count; ++Index)
		{
			FGearBone& Bone = Bones[Index];
			if (Bone.bRotationDirty != 0)
			{
				Bone.Local.M = GearBoneRotation(Bone.Angles, Bone.RotationOrder).M;
				Bone.bRotationDirty = 0;
			}
			Place(Bones, Bone);
		}
		for (std::size_t Index = 1; Index < Count; ++Index)
		{
			Bones[Index].bWorldDirty = 0;
		}
	}

	bool ComputeGearBonesScaled(std::span<FGearBone> Bones, int32 ModelScale)
	{
		FGearBone& Root = Bones[0];
		ComputeRoot(Root, ModelScale);
		const std::size_t Count = Root.Index;
		for (std::size_t Index = 1; Index < Count; ++Index)
		{
			FGearBone& Bone = Bones[Index];
			const FGearBone* Parent = Bone.Parent == FGearBone::NoParent ? nullptr : &Bones[static_cast<std::size_t>(Bone.Parent)];
			if (Parent != nullptr && Parent->bRotationDirty == 1)
			{
				Bone.bRotationDirty = 1;
			}
			if (Bone.bRotationDirty != 0)
			{
				FGteMatrix Rotation = GearBoneRotation(Bone.Angles, Bone.RotationOrder);
				Bone.Local.M = MulMatrix(Rotation, Diagonal(Bone.Scale[0], Bone.Scale[1], Bone.Scale[2])).M;
				if (Parent != nullptr)
				{
					// A child undoes its parent's scale, which its parent's world matrix carries.
					if (Parent->Scale[0] == 0 || Parent->Scale[1] == 0 || Parent->Scale[2] == 0)
					{
						return false;
					}
					const FGteMatrix Unscale = Diagonal(0x1000000 / Parent->Scale[0], 0x1000000 / Parent->Scale[1], 0x1000000 / Parent->Scale[2]);
					Bone.Local.M = MulMatrix(Unscale, Bone.Local).M;
				}
			}
			Place(Bones, Bone);
		}
		for (std::size_t Index = 1; Index < Count; ++Index)
		{
			Bones[Index].bWorldDirty = 0;
			Bones[Index].bRotationDirty = 0;
		}
		return true;
	}
}
