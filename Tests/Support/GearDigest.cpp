#include "Support/GearDigest.h"

#include "Field/EventContext.h"
#include "Gears/GearModels.h"

namespace Weltall::Testing
{
	namespace
	{
		int32 CodeRef(const FGearCodeRef& Ref)
		{
			return Ref ? Ref.Pack->Slot << 24 | static_cast<int32>(Ref.Offset) : -1;
		}

		void DigestMatrix32(FOracleDigest& Digest, const FGteMatrix& Matrix)
		{
			for (const auto& Row : Matrix.M)
			{
				Digest.I32(Row[0], Row[1], Row[2]);
			}
			Digest.I32(Matrix.T[0], Matrix.T[1], Matrix.T[2]);
		}
	}

	void DigestGearModel(FOracleDigest& Digest, const FGearModel& Model)
	{
		Digest.I32(Model.Scale, Model.Slot, Model.ShadowHalfWidth, Model.ShadowHalfDepth, Model.DefaultRecord, Model.Queue);
		for (const uint8 Owner : Model.QueuedOwners)
		{
			Digest.I32(Owner);
		}
		for (const uint8 Animation : Model.QueuedAnimations)
		{
			Digest.I32(Animation);
		}
		Digest.I32(Model.bShown, Model.FreeHeight, Model.ScaledBones, Model.Signal, Model.Mark, static_cast<int16>(Model.WatchTag), Model.WindY,
			static_cast<int16>(Model.WaitTicks), static_cast<int16>(Model.Loops), static_cast<int16>(Model.Timer), static_cast<int16>(Model.TimerEnd),
			Model.NearDistance, static_cast<int16>(Model.Flags));
		Digest.I32(CodeRef(Model.Pc), CodeRef(Model.OnNear), CodeRef(Model.OnTimer), CodeRef(Model.OnLanding));
		Digest.I32(Model.HostSlot, Model.HostTurns, static_cast<int16>(Model.HostBone), Model.FloorY);
		for (const std::array<int16, 3>* Vector : {&Model.AttachOffset, &Model.Spin, &Model.SpinAcceleration, &Model.Velocity, &Model.Acceleration, &Model.Target})
		{
			Digest.I32((*Vector)[0], (*Vector)[1], (*Vector)[2]);
		}
		Digest.I32(Model.Speed, Model.TextureX, Model.TextureY, Model.ClutX, Model.ClutY);
		Digest.I32(Model.TimelineTick, Model.TimelineLoop, Model.TimelineDone, Model.TimelineEvents, CodeRef(Model.NextEvent), CodeRef(Model.FirstEvent));
		Digest.I32(Model.PlayMask, Model.Trails, static_cast<uint8>(Model.Cloths.size()), Model.TextureAnimations);
		Digest.I32(Model.RootBefore[0], Model.RootBefore[1], Model.RootBefore[2], Model.RootMotion[0], Model.RootMotion[1], Model.RootMotion[2]);
	}

	void DigestGearBone(FOracleDigest& Digest, const FGearBone& Bone)
	{
		Digest.I32(Bone.Parent, Bone.bWorldDirty, Bone.bRotationDirty, Bone.RotationOrder, Bone.bShown, Bone.Part, Bone.Index);
		DigestMatrix32(Digest, Bone.Local);
		DigestMatrix32(Digest, Bone.World);
		Digest.I32(Bone.Scale[0], Bone.Scale[1], Bone.Scale[2], Bone.Billboard, Bone.Angles[0], Bone.Angles[1], Bone.Angles[2]);
		Digest.I32(Bone.Position[0], Bone.Position[1], Bone.Position[2], Bone.Rotation, Bone.Translation, Bone.Scaling);
	}

	void DigestGearCloths(FOracleDigest& Digest, const FGearModel& Model)
	{
		for (const FGearCloth& Cloth : Model.Cloths)
		{
			for (const FGearClothPoint& Point : Cloth.Points)
			{
				Digest.I32(Point.Position[0], Point.Position[1], Point.Position[2], Point.Normal[0], Point.Normal[1], Point.Normal[2]);
			}
		}
	}

	void DigestGearPool(FOracleDigest& Digest, const FGearChannelPool& Pool)
	{
		Digest.I32(Pool.LowestFree(), Pool.Capacity());
		for (int32 Index = 0; Index < Pool.Capacity(); ++Index)
		{
			const FGearChannel& Channel = Pool[Index];
			if (!Channel.bUsed)
			{
				Digest.I32(0);
				continue;
			}
			Digest.I32(1, Channel.Loop, Channel.Kind, Channel.Tag);
			if (Channel.IsStream())
			{
				Digest.I32(CodeRef(Channel.StreamStart), CodeRef(Channel.StreamRead));
			}
			else
			{
				for (const int16 Word : Channel.Words)
				{
					Digest.I32(Word);
				}
			}
			Digest.I32(Channel.Tick, Channel.Length);
		}
	}

	void DigestGearModels(FOracleDigest& Digest, const FGearModels& Models)
	{
		Digest.I32(Models.VblankCounter, Models.PlayMask, Models.PlaySlot, Models.Wind, Models.WindPhase, Models.bSnapPoses ? 1 : 0);
		for (int32 Slot = 0; Slot < FGearModels::Slots; ++Slot)
		{
			if (const FGearModel* Model = Models.Model(Slot))
			{
				DigestGearModel(Digest, *Model);
				for (std::size_t Bone = 0; Bone < Model->Bones[0].Index; ++Bone)
				{
					DigestGearBone(Digest, Model->Bones[Bone]);
				}
				DigestGearCloths(Digest, *Model);
			}
		}
		DigestGearPool(Digest, Models.Pool());
	}

	void DigestFieldGears(FOracleDigest& Digest, const FEventContext& Context)
	{
		const FFieldGears& Gears = Context.Gears;
		if (!Gears.Models)
		{
			return;
		}
		Digest.I32(Gears.Count);
		for (const uint16 File : Gears.FileNumbers)
		{
			Digest.I16(File);
		}
		for (const int16 Animation : Gears.Animations)
		{
			Digest.I16(Animation);
		}
		for (const int32 Scale : Gears.LoadScales)
		{
			Digest.I32(Scale);
		}
		for (const uint8 Shift : Gears.TextureShifts)
		{
			Digest.U8(Shift);
		}
		Digest.I16(Gears.PoolSize);
		DigestGearModels(Digest, *Gears.Models);
		for (const FGearUpload& Upload : Gears.Models->Uploads())
		{
			Digest.I16(Upload.X, Upload.Y, Upload.Width, Upload.Height);
		}
		for (const FGearVramMove& Move : Gears.Models->VramMoves())
		{
			Digest.I16(Move.X, Move.Y, Move.Width, Move.Height, Move.ToX, Move.ToY);
		}
	}
}
