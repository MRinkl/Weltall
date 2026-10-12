#pragma once

#include "Gears/GearChannels.h"
#include "Gears/GearEngine.h"
#include "Gears/GearFiles.h"
#include "Graphics/Gte.h"
#include "Graphics/GteVectors.h"

#include <array>
#include <optional>

/**
 * The gears' engine's inner steps, the same code in every program that hosts it: reading motion packs,
 * the guarded arithmetic, the channels and the poses, a tick of a model's channels. Each records a
 * fault in its engine where the original breaks.
 */
namespace Weltall::GearMachine
{
	/** An animation record's header, read from its motion pack. */
	struct FGearRecord
	{
		FGearCodeRef At;
		int16 Length = 0;
		uint16 Flags = 0;
		bool bPoseOnly = false;
		uint16 Rotations = 0;
		uint16 Translations = 0;
		int16 Stride = 0;
		int16 Events = 0;
		uint32 EventOffset = 0;

		static constexpr uint16 NoRotationPose = 1;
		static constexpr uint16 NoTranslationPose = 2;
		static constexpr uint32 HeaderBytes = 0x18;
		static constexpr uint32 DescriptorBytes = 6;

		/** Where the pose starts: after the channel descriptors, which a pose record has none of. */
		FGearCodeRef Pose() const { return At + static_cast<int32>(HeaderBytes + (bPoseOnly ? 0 : DescriptorBytes * (Rotations + 1u))); }
	};

	// Reading and writing a motion pack; a place outside it (or in a pack the host no longer holds) faults and reads 0.
	uint8 ReadU8(FGearEngine& Engine, const FGearCodeRef& At);
	uint16 ReadU16(FGearEngine& Engine, const FGearCodeRef& At);
	int16 ReadS16(FGearEngine& Engine, const FGearCodeRef& At);
	uint32 ReadU32(FGearEngine& Engine, const FGearCodeRef& At);
	void WriteU16(FGearEngine& Engine, const FGearCodeRef& At, uint16 Value);
	std::optional<FGearRecord> ReadRecord(FGearEngine& Engine, const FGearCodeRef& At);

	/** The R3000's 32-bit sums and products, which wrap. */
	inline int32 WrapAdd(int32 A, int32 B)
	{
		return static_cast<int32>(static_cast<uint32>(A) + static_cast<uint32>(B));
	}

	inline int32 WrapSub(int32 A, int32 B)
	{
		return static_cast<int32>(static_cast<uint32>(A) - static_cast<uint32>(B));
	}

	inline int32 WrapMul(int32 A, int32 B)
	{
		return static_cast<int32>(static_cast<uint32>(A) * static_cast<uint32>(B));
	}

	/** A C division the compiler guards: a zero divisor (or the one overflow) faults and gives 0. */
	int32 Divide(FGearEngine& Engine, int32 Dividend, int32 Divisor);
	/** libgte's SquareRoot0 of three squares; a sum past 31 bits faults. */
	int32 Distance(FGearEngine& Engine, int32 X, int32 Y, int32 Z);
	int32 Ratan2(FGearEngine& Engine, int32 Y, int32 X);

	FGearBone* BoneAt(FGearEngine& Engine, FGearBody& Body, int32 Bone);
	/** From the root to the body's target point. */
	int32 TargetDistance(FGearEngine& Engine, const FGearBody& Body);

	// Channels and poses (GearPoses.cpp).
	int16 AllocateChannel(FGearEngine& Engine);
	void FreeChannel(FGearEngine& Engine, int16& Channel);
	/** Every channel of every bone but the locked ones. */
	void FreeUnlockedChannels(FGearEngine& Engine, FGearBody& Body);
	void FreeTaggedChannels(FGearEngine& Engine, FGearBody& Body, uint8 Tag);
	/** Bone's channels named by Which (1 rotation, 2 translation, 4 scale), whatever their tags. */
	void FreeBoneChannels(FGearEngine& Engine, FGearBody& Body, int32 Bone, uint32 Which);

	void PlayRecord(FGearEngine& Engine, FGearBody& Body, const FGearRecord& Record, uint32 Loop, uint8 Tag);
	void SnapPose(FGearEngine& Engine, FGearBody& Body, const FGearRecord& Record);
	void BlendToPose(FGearEngine& Engine, FGearBody& Body, const FGearRecord& Record, int32 Ticks, uint32 Kind, uint32 Loop, uint8 Tag);
	void RotateRootTo(FGearEngine& Engine, FGearBody& Body, int32 Ticks, int32 X, int32 Y, int32 Z);
	void TurnRootToward(FGearEngine& Engine, FGearBody& Body, uint8 YawOnly, int16 Gain, int16 Speed, int16 Rate);
	/** A script's channel on a bone: Words are from (0-2) and to (3-5). */
	void SetChannel(FGearEngine& Engine, FGearBody& Body, int32 Bone, uint32 Flags, uint32 Kind, uint32 Tag, uint32 Loop, const std::array<int16, 6>& Words, int16 Ticks);
	void SetBone(FGearEngine& Engine, FGearBody& Body, int32 Bone, uint32 Flags, int16 X, int16 Y, int16 Z);
	void ShowBone(FGearEngine& Engine, FGearBody& Body, int32 Bone, uint32 Flags);

	/** Every bone's channels stepped once (GearChannelStep.cpp); the events word the scripts wait on. */
	uint32 StepChannels(FGearEngine& Engine, FGearBody& Body);

	/**
	 * An attachment's offset along one axis (GearAttachments.cpp): Offset projected on the normal of First
	 * and Second, as the scripts that keep an attached model where it stands work it out.
	 */
	int16 ProjectOnAxis(FGearEngine& Engine, const std::array<int32, 3>& Offset, const FGteLongVector& First, const FGteLongVector& Second, int32 Scale);
}
