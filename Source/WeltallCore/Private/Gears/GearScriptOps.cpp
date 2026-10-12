#include "GearScript.h"

namespace Weltall::GearMachine
{
	namespace
	{
		constexpr uint32 AllChannels = 7;
		constexpr uint32 RotationChannel = 1;
		constexpr uint32 TranslationChannel = 2;
		constexpr uint8 RelativeToTexture = 1;

		void StopTimeline(FGearModel& Model)
		{
			Model.TimelineTick = -1;
		}

		void FreeChannelsOp(FGearModels& Models, FGearScriptRun&, FGearModel& Model)
		{
			FreeUnlockedChannels(Models, Model);
			StopTimeline(Model);
		}

		template <uint32 Which>
		void FreeBoneChannelsOp(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			FreeBoneChannels(Models, Model, Run.B, Which);
		}

		void ResetPose(FGearModels& Models, FGearScriptRun&, FGearModel& Model)
		{
			FreeUnlockedChannels(Models, Model);
			for (std::size_t Index = 1; Index < Model.Bones[0].Index; ++Index)
			{
				FGearBone& Bone = Model.Bones[Index];
				Bone.Angles = {};
				Bone.Position = {};
				Bone.bWorldDirty = 1;
				Bone.bRotationDirty = 1;
			}
		}

		void SnapPoseOp(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			bool bFlag = false;
			if (const std::optional<FGearRecord> Found = UsedRecord(Models, Model, Run.B, bFlag))
			{
				SnapPose(Models, Model, *Found);
			}
		}

		void PlayRecordOp(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			bool bFlag = false;
			const std::optional<FGearRecord> Found = Record(Models, Model, Run.B, bFlag);
			const uint16 Word = ReadU16(Models, Run.Word(1));
			Run.Next = Run.At + 4;
			if (bFlag)
			{
				return;
			}
			if (!Found)
			{
				UsedRecord(Models, Model, Run.B, bFlag);
				return;
			}
			const uint32 Loop = Word >> 8;
			PlayRecord(Models, Model, *Found, Loop, static_cast<uint8>(Word));
			Run.Events = -1;
			const int32 Speed = Found->Stride * ((Model.Scale * Model.Bones[0].Scale[2]) >> 12) >> 12;
			Model.Speed = static_cast<int16>(Speed < 0 ? -Speed : Speed);
			StartTimeline(Models, Model, *Found, static_cast<int32>(Loop));
		}

		void BlendToPoseOp(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			const uint16 First = ReadU16(Models, Run.Word(1));
			const uint16 Second = ReadU16(Models, Run.Word(2));
			Run.Next = Run.At + 6;
			bool bFlag = false;
			if (const std::optional<FGearRecord> Found = UsedRecord(Models, Model, First & 0xFF, bFlag))
			{
				if (Models.bSnapPoses)
				{
					SnapPose(Models, Model, *Found);
				}
				else
				{
					BlendToPose(Models, Model, *Found, Second >> 8, Run.B, Second & 0xFF, static_cast<uint8>(First >> 8));
				}
			}
			Run.Events = -1;
			StopTimeline(Model);
		}

		void StartTimelineOp(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			bool bFlag = false;
			const std::optional<FGearRecord> Found = UsedRecord(Models, Model, Run.B, bFlag);
			const int16 Loop = ReadS16(Models, Run.Word(1));
			Run.Next = Run.At + 4;
			if (Found)
			{
				StartTimeline(Models, Model, *Found, Loop);
			}
		}

		void StopTimelineOp(FGearModels&, FGearScriptRun&, FGearModel& Model)
		{
			StopTimeline(Model);
		}

		void SetChannelOp(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			Run.Events = -1;
			const uint16 First = ReadU16(Models, Run.Word(1));
			const uint16 Second = ReadU16(Models, Run.Word(2));
			std::array<int16, 6> Words{};
			for (int32 Index = 0; Index < 6; ++Index)
			{
				Words[static_cast<std::size_t>(Index)] = ReadS16(Models, Run.Word(3 + Index));
			}
			const int16 Ticks = ReadS16(Models, Run.Word(9));
			Run.Next = Run.At + 20;
			SetChannel(Models, Model, Run.B, First & 0xFF, First >> 8, Second & 0xFF, Second >> 8, Words, Ticks);
		}

		void SetScaledBones(FGearModels&, FGearScriptRun& Run, FGearModel& Model)
		{
			Model.ScaledBones = Run.B;
		}

		void ShowBoneOp(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			const int16 Bone = ReadS16(Models, Run.Word(1));
			Run.Next = Run.At + 4;
			ShowBone(Models, Model, Bone, Run.B);
		}

		void ComputeBonesOp(FGearModels& Models, FGearScriptRun&, FGearModel& Model)
		{
			if (Model.ScaledBones == 0)
			{
				ComputeGearBones(Model.Bones, Model.Scale);
			}
			else if (!ComputeGearBonesScaled(Model.Bones, Model.Scale))
			{
				Models.Fail("a gear's scaled bones divide by a parent's scale of 0");
			}
		}

		void SetBillboard(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			const int16 Bone = ReadS16(Models, Run.Word(1));
			Run.Next = Run.At + 4;
			if (FGearBone* Target = BoneAt(Models, Model, Bone))
			{
				Target->Billboard = Run.B;
			}
		}

		void SetBoneOp(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			const int16 Bone = ReadS16(Models, Run.Word(1));
			const int16 X = ReadS16(Models, Run.Word(2));
			const int16 Y = ReadS16(Models, Run.Word(3));
			const int16 Z = ReadS16(Models, Run.Word(4));
			Run.Next = Run.At + 10;
			SetBone(Models, Model, Bone, Run.B, X, Y, Z);
		}

		/** A timeline of the next B events, which the instruction jumps over. */
		void InlineTimeline(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			Model.TimelineTick = 0;
			Model.TimelineLoop = -1;
			Model.TimelineDone = 0;
			Model.TimelineEvents = Run.B;
			Model.NextEvent = Run.At + 4;
			Run.Next = Run.At + ReadS16(Models, Run.Word(1));
		}

		void SetRotationOrder(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			const int16 Bone = ReadS16(Models, Run.Word(1));
			Run.Next = Run.At + 4;
			if (FGearBone* Target = BoneAt(Models, Model, Bone))
			{
				Target->RotationOrder = Run.B;
			}
		}

		void MoveImage(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			FGearVramMove Move;
			Move.X = ReadS16(Models, Run.Word(1));
			Move.Y = ReadS16(Models, Run.Word(2));
			Move.ToX = ReadS16(Models, Run.Word(3));
			Move.ToY = ReadS16(Models, Run.Word(4));
			// The width rounded to an even count toward zero after one is added, as the compiler halves and doubles it.
			const int32 Wide = static_cast<int16>(ReadS16(Models, Run.Word(5)) + 1);
			Move.Width = static_cast<int16>((Wide - (Wide >> 31)) & 0xFFFE);
			Move.Height = ReadS16(Models, Run.Word(6));
			Run.Next = Run.At + 14;
			if ((Run.B & RelativeToTexture) != 0)
			{
				if (Model.TextureX < 0)
				{
					return;
				}
				Move.X = static_cast<int16>(Move.X + Model.TextureX);
				Move.Y = static_cast<int16>(Move.Y + Model.TextureY);
				Move.ToX = static_cast<int16>(Move.ToX + Model.TextureX);
				Move.ToY = static_cast<int16>(Move.ToY + Model.TextureY);
			}
			Models.PendingMoves.push_back(Move);
		}

		void Show(FGearModels&, FGearScriptRun& Run, FGearModel& Model)
		{
			Model.bShown = Run.B & 1;
		}

		void FadeEffect(FGearModels& Models, FGearScriptRun&, FGearModel&)
		{
			Models.Fail("a gear's script fades a sound effect, which the port's gears do not play");
		}

		void SetScale(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			Model.Scale = ReadS16(Models, Run.Word(1));
			Run.Next = Run.At + 4;
		}

		void SetFlags(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			Model.Flags = ReadU16(Models, Run.Word(1));
			Run.Next = Run.At + 4;
		}

		void SetWind(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model)
		{
			Model.WindY = ReadS16(Models, Run.Word(1));
			Run.Next = Run.At + 4;
		}
	}

	void AddChannelOps(std::array<FGearOp, 0x71>& Ops)
	{
		Ops[0x08] = &FreeChannelsOp;
		Ops[0x0A] = &FreeBoneChannelsOp<AllChannels>;
		Ops[0x0B] = &ResetPose;
		Ops[0x0D] = &FreeBoneChannelsOp<RotationChannel>;
		Ops[0x0E] = &FreeBoneChannelsOp<TranslationChannel>;
		Ops[0x10] = &SnapPoseOp;
		Ops[0x11] = &PlayRecordOp;
		Ops[0x13] = &BlendToPoseOp;
		Ops[0x18] = &StartTimelineOp;
		Ops[0x19] = &StopTimelineOp;
		Ops[0x1D] = &SetChannelOp;
		Ops[0x1E] = &SetScaledBones;
		Ops[0x23] = &ShowBoneOp;
		Ops[0x27] = &ComputeBonesOp;
		Ops[0x5D] = &SetBillboard;
		Ops[0x62] = &SetBoneOp;
		Ops[0x63] = &InlineTimeline;
		Ops[0x6B] = &SetRotationOrder;
	}

	void AddModelOps(std::array<FGearOp, 0x71>& Ops)
	{
		Ops[0x1A] = &MoveImage;
		Ops[0x24] = &Show;
		Ops[0x3C] = &FadeEffect;
		Ops[0x5E] = &SetScale;
		Ops[0x5F] = &SetFlags;
		Ops[0x64] = &SetWind;
	}
}
