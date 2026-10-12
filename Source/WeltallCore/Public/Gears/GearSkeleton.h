#pragma once

#include "Foundation/Types.h"
#include "Graphics/Gte.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	struct FGearSkeletonEntry;

	/**
	 * One bone of a gear's skeleton, 7Ch bytes in the original. Bone 0 is the root: its local matrix is
	 * its rotation times the model's scale with its position, its world matrix its rotation alone; every
	 * other bone's world matrix is in the model's frame, its parent's world matrix times its own local
	 * one (or its local one without a parent: no bone's parent is the root).
	 */
	struct FGearBone
	{
		static constexpr int16 NoParent = -1;
		static constexpr uint16 NoPart = 0xFFFF;
		static constexpr int16 NoChannel = -1;

		int16 Parent = NoParent;
		uint8 bWorldDirty = 0;
		uint8 bRotationDirty = 0;
		/** 0: the kernel's rotation (x, y, z); otherwise libgte's y, x, z. */
		uint8 RotationOrder = 0;
		uint8 bShown = 0;
		uint16 Part = NoPart;
		/** The bone's number; the root's is the bone count. */
		uint16 Index = 0;
		FGteMatrix Local;
		FGteMatrix World;
		std::array<int16, 3> Scale{};
		/** 0 none; 1 turned about the up axis toward the camera; more: facing it. */
		int16 Billboard = 0;
		FGteVector Angles{};
		std::array<int32, 3> Position{};
		int16 Rotation = NoChannel;
		int16 Translation = NoChannel;
		int16 Scaling = NoChannel;
	};

	/**
	 * The bones' matrices for this frame as the model's update computes them before its channels step:
	 * the root's from its angles, scale and position, then each bone whose rotation or placement changed
	 * (or whose parent's did) in order.
	 */
	WELTALLCORE_API void ComputeGearBones(std::span<FGearBone> Bones, int32 ModelScale);

	/**
	 * The same with each bone's own scale in its local matrix, undone for its children: false where a
	 * parent's scale of 0 would divide by zero (the original traps).
	 */
	[[nodiscard]] WELTALLCORE_API bool ComputeGearBonesScaled(std::span<FGearBone> Bones, int32 ModelScale);

	/** The rotation a bone's angles give in its order. */
	WELTALLCORE_API FGteMatrix GearBoneRotation(const FGteVector& Angles, uint8 RotationOrder);

	/**
	 * A skeleton as a load makes it (the room of Bones reused): the root, turning in libgte's y, x, z
	 * order, then a bone per entry, each unscaled, shown and to be computed.
	 */
	WELTALLCORE_API void MakeGearSkeleton(std::span<const FGearSkeletonEntry> Skeleton, std::vector<FGearBone>& Bones);
}
