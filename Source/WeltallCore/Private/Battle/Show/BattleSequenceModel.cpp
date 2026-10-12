#include "BattleObjectSequence.h"

#include <string>

namespace Weltall::BattleObjectMachine
{
	namespace
	{
		constexpr uint32 AllTracks = 7;
		constexpr uint32 RotationTrack = 1;
		constexpr uint32 TranslationTrack = 2;
		constexpr uint8 RelativeToTexture = 1;
		constexpr uint8 RelativeClut = 0x80;
		constexpr uint8 NoFeed = 0xFF;
		/** Op 1B asks for every buffer its type takes. */
		constexpr uint32 EveryBuffer = 0x700;
		constexpr uint32 FromVram = 1;
		constexpr uint32 NoStream = 0xFFFF;
		constexpr int32 ClutStartBytes = 32;

		void StopKeyEvents(FBattleObject& Object)
		{
			Object.KeyFrame = -1;
		}

		/** The animation an instruction plays: none faults, as the original would play what lies past its tables. */
		std::optional<FGearRecord> UsedAnimation(FBattleObjects& Objects, FBattleObject& Object, uint32 Number, bool& bMirrored)
		{
			std::optional<FGearRecord> Found = Animation(Objects, Object, Number, bMirrored);
			if (!Found && !Objects.Faulted())
			{
				Objects.Fail("object " + std::to_string(Object.Slot) + " plays animation " + std::to_string(Number) + ", past its tables");
			}
			return Found;
		}

		/** The animation's speed along its way: its stride in the object's scale, unsigned. */
		void TakeStride(FBattleObject& Object, const FGearRecord& Record)
		{
			const int32 Speed = Record.Stride * ((Object.Scale * Object.Bones[0].Scale[2]) >> 12) >> 12;
			Object.Speed = static_cast<int16>(Speed < 0 ? -Speed : Speed);
		}

		/**
		 * A record started: its streams from their first step, each bone but the root also
		 * put at once where the record's pose places it, the pose's entries taken in turn as the bones
		 * with streams (and without a locked track) ask for them.
		 */
		void PlayRecordAt(FBattleObjects& Objects, FBattleObject& Object, const FGearRecord& Record, uint32 Loop, uint8 Tag)
		{
			if (Record.bPoseOnly)
			{
				GearMachine::FreeUnlockedChannels(Objects, Object);
				GearMachine::SnapPose(Objects, Object, Record);
				return;
			}
			Loop &= 1;
			const std::size_t Count = std::min<std::size_t>(Object.Bones[0].Index, Record.Rotations + 1u);
			const int16 Length = static_cast<int16>(Loop != 0 ? Record.Length : Record.Length - 1);
			const FGearCodeRef Descriptors = Record.At + static_cast<int32>(FGearRecord::HeaderBytes);
			FGearCodeRef Pose = Record.At + static_cast<int32>(FGearRecord::HeaderBytes + FGearRecord::DescriptorBytes * (Record.Rotations + 1u));
			FGearCodeRef Streams = Pose;
			if ((Record.Flags & FGearRecord::NoRotationPose) == 0)
			{
				Streams = Streams + static_cast<int32>(FGearRecord::DescriptorBytes * Record.Rotations);
			}
			if ((Record.Flags & FGearRecord::NoTranslationPose) == 0)
			{
				Streams = Streams + static_cast<int32>(FGearRecord::DescriptorBytes * Record.Translations);
			}
			uint32 Rotations = 0;
			uint32 Translations = 0;
			for (std::size_t Index = 0; Index < Count && !Objects.Faulted(); ++Index)
			{
				const FGearCodeRef Descriptor = Descriptors + static_cast<int32>(FGearRecord::DescriptorBytes * Index);
				FGearBone& Bone = Object.Bones[Index];
				for (const bool bTranslation : {false, true})
				{
					int16& Current = bTranslation ? Bone.Translation : Bone.Rotation;
					const bool bPosed = (Record.Flags & (bTranslation ? FGearRecord::NoTranslationPose : FGearRecord::NoRotationPose)) == 0 && Index != 0 &&
						(bTranslation ? Translations < Record.Translations : Rotations < Record.Rotations);
					const uint32 Stream = GearMachine::ReadU16(Objects, Descriptor + (bTranslation ? 2 : 0));
					const bool bLocked = Current != FGearBone::NoChannel && Objects.Channels[Current].Tag == GearChannelTag::Locked;
					if (Stream == NoStream || bLocked)
					{
						// The pose's entry is passed over.
						if (bPosed)
						{
							Pose = Pose + 6;
							++(bTranslation ? Translations : Rotations);
						}
						continue;
					}
					const int16 Channel = Current != FGearBone::NoChannel ? Current : GearMachine::AllocateChannel(Objects);
					if (bPosed)
					{
						const std::array<int16, 3> Values = {GearMachine::ReadS16(Objects, Pose), GearMachine::ReadS16(Objects, Pose + 2),
							GearMachine::ReadS16(Objects, Pose + 4)};
						Pose = Pose + 6;
						Bone.bWorldDirty = 1;
						if (bTranslation)
						{
							++Translations;
							Bone.Position = {Values[0], Values[1], Values[2]};
						}
						else
						{
							++Rotations;
							Bone.Angles = {Values[0], Values[1], Values[2]};
							Bone.bRotationDirty = 1;
						}
					}
					if (Channel == FGearBone::NoChannel)
					{
						continue;
					}
					FGearChannel& Track = Objects.Channels[Channel];
					Track.bUsed = true;
					Track.Loop = static_cast<uint8>(Loop);
					Track.Tag = Tag;
					Track.Kind = GearMachine::ReadU8(Objects, Descriptor + (bTranslation ? 5 : 4));
					Track.Tick = 0;
					Track.Length = Length;
					Track.StreamStart = Streams + static_cast<int32>(Stream);
					Track.StreamRead = Track.StreamStart;
					Current = Channel;
				}
			}
		}

		void StopTracksOp(FBattleObjects& Objects, FSequenceRun&, FBattleObject& Object)
		{
			GearMachine::FreeUnlockedChannels(Objects, Object);
			StopKeyEvents(Object);
		}

		void StopOwnerTracks(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			GearMachine::FreeTaggedChannels(Objects, Object, Run.B);
		}

		template <uint32 Which>
		void StopBoneTracks(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			GearMachine::FreeBoneChannels(Objects, Object, Run.B, Which);
		}

		void ResetPose(FBattleObjects& Objects, FSequenceRun&, FBattleObject& Object)
		{
			GearMachine::FreeUnlockedChannels(Objects, Object);
			for (std::size_t Index = 1; Index < Object.Bones[0].Index; ++Index)
			{
				FGearBone& Bone = Object.Bones[Index];
				Bone.Angles = {};
				Bone.Position = {};
				Bone.bWorldDirty = 1;
				Bone.bRotationDirty = 1;
			}
		}

		void SetPoseOp(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			bool bMirrored = false;
			if (const std::optional<FGearRecord> Found = UsedAnimation(Objects, Object, Run.B, bMirrored))
			{
				GearMachine::SnapPose(Objects, Object, *Found);
			}
		}

		/** 11, 12: the animation from its start with its key events, or with its pose put at once and none. */
		template <bool bAtPose>
		void PlayAnimation(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			bool bMirrored = false;
			const std::optional<FGearRecord> Found = Animation(Objects, Object, Run.B, bMirrored);
			const uint16 Operand = Word(Objects, Run, 1);
			Run.Next = Run.At + 4;
			if (bMirrored)
			{
				return;
			}
			if (!Found)
			{
				UsedAnimation(Objects, Object, Run.B, bMirrored);
				return;
			}
			const uint32 Loop = Operand >> 8;
			if (bAtPose)
			{
				PlayRecordAt(Objects, Object, *Found, Loop, static_cast<uint8>(Operand));
			}
			else
			{
				GearMachine::PlayRecord(Objects, Object, *Found, Loop, static_cast<uint8>(Operand));
			}
			Run.Events = -1;
			TakeStride(Object, *Found);
			if (!bAtPose)
			{
				StartKeyEvents(Object, *Found, static_cast<int32>(Loop));
			}
		}

		void BlendToPoseOp(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const uint16 First = Word(Objects, Run, 1);
			Run.Events = -1;
			const uint16 Second = Word(Objects, Run, 2);
			Run.Next = Run.At + 6;
			bool bMirrored = false;
			if (const std::optional<FGearRecord> Found = UsedAnimation(Objects, Object, First & 0xFF, bMirrored))
			{
				GearMachine::BlendToPose(Objects, Object, *Found, Second >> 8, Run.B, Second & 0xFF, static_cast<uint8>(First >> 8));
			}
			StopKeyEvents(Object);
		}

		void StartKeyEventsOp(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			bool bMirrored = false;
			const std::optional<FGearRecord> Found = UsedAnimation(Objects, Object, Run.B, bMirrored);
			const int16 Loop = SignedWord(Objects, Run, 1);
			Run.Next = Run.At + 4;
			if (Found)
			{
				StartKeyEvents(Object, *Found, Loop);
			}
		}

		void StopKeyEventsOp(FBattleObjects&, FSequenceRun&, FBattleObject& Object)
		{
			StopKeyEvents(Object);
		}

		void MoveImage(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			int16 X = SignedWord(Objects, Run, 1);
			int16 Y = SignedWord(Objects, Run, 2);
			int16 ToX = SignedWord(Objects, Run, 3);
			int16 ToY = SignedWord(Objects, Run, 4);
			// The width rounded to an even count toward zero after one is added, as the compiler halves and doubles it.
			const int32 Wide = static_cast<int16>(SignedWord(Objects, Run, 5) + 1);
			const int16 Width = static_cast<int16>((Wide - (Wide >> 31)) & 0xFFFE);
			const int16 Height = SignedWord(Objects, Run, 6);
			Run.Next = Run.At + 14;
			if ((Run.B & RelativeToTexture) != 0)
			{
				if (Object.TextureX < 0)
				{
					return;
				}
				X = static_cast<int16>(X + Object.TextureX);
				Y = static_cast<int16>(Y + Object.TextureY);
				ToX = static_cast<int16>(ToX + Object.TextureX);
				ToY = static_cast<int16>(ToY + Object.TextureY);
			}
			Objects.Services->MoveImage({X, Y, Width, Height}, ToX, ToY);
		}

		void StartClut(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			Run.Next = Run.At + ClutStartBytes;
			const int32 Count = static_cast<int32>(Object.ClutAnimations.size());
			if (Run.B >= Count)
			{
				return;
			}
			const uint16 Kinds = Word(Objects, Run, 1);
			const uint8 Fed = static_cast<uint8>(Kinds);
			const uint8 Type = static_cast<uint8>(Kinds >> 8);
			const uint16 Sources = Word(Objects, Run, 2);
			FBattleClutStart Start;
			Start.Type = Type & 0x7F;
			Start.Flags = (Sources & 0xFF) | EveryBuffer;
			Start.Feeds = Fed == NoFeed || Fed >= Count ? int16{-1} : static_cast<int16>(Fed);
			Start.bColourMatrix = Start.Type > 3;
			Start.Curve = ClutCurve(Sources >> 8);
			for (int32 Value = 0; Value < 6; ++Value)
			{
				Start.Values[static_cast<std::size_t>(Value)] = Word(Objects, Run, 3 + Value);
			}
			Start.X = SignedWord(Objects, Run, 9);
			Start.Y = SignedWord(Objects, Run, 10);
			Start.Width = SignedWord(Objects, Run, 11);
			Start.Height = SignedWord(Objects, Run, 12);
			Start.Speed = SignedWord(Objects, Run, 13);
			Start.Divisor = SignedWord(Objects, Run, 14);
			Start.Base = SignedWord(Objects, Run, 15);
			if ((Type & RelativeClut) != 0)
			{
				if (Object.TextureX < 0)
				{
					return;
				}
				Start.X = static_cast<int16>(Start.X + Object.ClutX);
				Start.Y = static_cast<int16>(Start.Y + Object.ClutY);
				if ((Sources & 0xF) == FromVram)
				{
					Start.Values[0] = static_cast<uint16>(Start.Values[0] + Object.ClutX);
					Start.Values[1] = static_cast<uint16>(Start.Values[1] + Object.ClutY);
				}
				if (((Sources & 0xFF) >> 4) == FromVram)
				{
					Start.Values[3] = static_cast<uint16>(Start.Values[3] + Object.ClutX);
					Start.Values[4] = static_cast<uint16>(Start.Values[4] + Object.ClutY);
				}
			}
			StartClutAnimation(Objects, Object.ClutAnimations[static_cast<std::size_t>(Run.B)], Start);
		}

		void StopClut(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			if (Run.B < Object.ClutAnimations.size())
			{
				StopClutAnimation(Objects, Object.ClutAnimations[Run.B]);
			}
		}

		void SetChannelOp(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			Run.Events = -1;
			const uint16 First = Word(Objects, Run, 1);
			const uint16 Second = Word(Objects, Run, 2);
			std::array<int16, 6> Words{};
			for (int32 Index = 0; Index < 6; ++Index)
			{
				Words[static_cast<std::size_t>(Index)] = SignedWord(Objects, Run, 3 + Index);
			}
			const int16 Ticks = SignedWord(Objects, Run, 9);
			Run.Next = Run.At + 20;
			GearMachine::SetChannel(Objects, Object, Run.B, First & 0xFF, First >> 8, Second & 0xFF, Second >> 8, Words, Ticks);
		}

		void SetScaledBones(FBattleObjects&, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.ScaledBones = Run.B;
		}

		void ShowBoneOp(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const int16 Bone = SignedWord(Objects, Run, 1);
			Run.Next = Run.At + 4;
			GearMachine::ShowBone(Objects, Object, Bone, Run.B);
		}

		void Show(FBattleObjects&, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.bVisible = Run.B & 1;
		}

		void ComputeBonesOp(FBattleObjects& Objects, FSequenceRun&, FBattleObject& Object)
		{
			if (Object.ScaledBones == 0)
			{
				ComputeGearBones(Object.Bones, Object.Scale);
			}
			else if (!ComputeGearBonesScaled(Object.Bones, Object.Scale))
			{
				Objects.Fail("object " + std::to_string(Object.Slot) + "'s scaled bones divide by a parent's scale of 0");
			}
		}

		void SetBillboard(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const int16 Bone = SignedWord(Objects, Run, 1);
			Run.Next = Run.At + 4;
			if (FGearBone* Target = GearMachine::BoneAt(Objects, Object, Bone))
			{
				Target->Billboard = Run.B;
			}
		}

		void SetScale(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.Scale = SignedWord(Objects, Run, 1);
			Run.Next = Run.At + 4;
		}

		void SetFlags(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.Flags = Word(Objects, Run, 1);
			Run.Next = Run.At + 4;
		}

		void SetBoneOp(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const int16 Bone = SignedWord(Objects, Run, 1);
			const int16 X = SignedWord(Objects, Run, 2);
			const int16 Y = SignedWord(Objects, Run, 3);
			const int16 Z = SignedWord(Objects, Run, 4);
			Run.Next = Run.At + 10;
			GearMachine::SetBone(Objects, Object, Bone, Run.B, X, Y, Z);
		}

		/** Key events of the next B records, which the instruction jumps over. */
		void InlineKeyEvents(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.KeyLoop = -1;
			Object.KeyFrame = 0;
			Object.KeyIndex = 0;
			Object.KeyCount = Run.B;
			Object.KeyEvents = Run.At + 4;
			Run.Next = JumpTarget(Objects, Run);
		}

		void SetWind(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			Object.WindY = SignedWord(Objects, Run, 1);
			Run.Next = Run.At + 4;
		}

		void SetRotationOrder(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object)
		{
			const int16 Bone = SignedWord(Objects, Run, 1);
			Run.Next = Run.At + 4;
			if (FGearBone* Target = GearMachine::BoneAt(Objects, Object, Bone))
			{
				Target->RotationOrder = Run.B;
			}
		}
	}

	void StartKeyEvents(FBattleObject& Object, const FGearRecord& Record, int32 Loop)
	{
		if (Record.Events == 0)
		{
			Object.KeyFrame = -1;
			return;
		}
		Object.KeyFrame = 0;
		Object.KeyLoop = Loop == 0 ? int16{-1} : Record.Length;
		Object.KeyIndex = 0;
		Object.KeyCount = Record.Events;
		Object.KeyEvents = Record.At + static_cast<int32>(Record.EventOffset);
		Object.KeyEventsLoop = Object.KeyEvents;
	}

	void AddSequenceModelOps(std::array<FSequenceOp, SequenceOpCount>& Ops)
	{
		Ops[0x08] = &StopTracksOp;
		Ops[0x09] = &StopOwnerTracks;
		Ops[0x0A] = &StopBoneTracks<AllTracks>;
		Ops[0x0B] = &ResetPose;
		Ops[0x0D] = &StopBoneTracks<RotationTrack>;
		Ops[0x0E] = &StopBoneTracks<TranslationTrack>;
		Ops[0x10] = &SetPoseOp;
		Ops[0x11] = &PlayAnimation<false>;
		Ops[0x12] = &PlayAnimation<true>;
		Ops[0x13] = &BlendToPoseOp;
		Ops[0x18] = &StartKeyEventsOp;
		Ops[0x19] = &StopKeyEventsOp;
		Ops[0x1A] = &MoveImage;
		Ops[0x1B] = &StartClut;
		Ops[0x1C] = &StopClut;
		Ops[0x1D] = &SetChannelOp;
		Ops[0x1E] = &SetScaledBones;
		Ops[0x23] = &ShowBoneOp;
		Ops[0x24] = &Show;
		Ops[0x27] = &ComputeBonesOp;
		Ops[0x5D] = &SetBillboard;
		Ops[0x5E] = &SetScale;
		Ops[0x5F] = &SetFlags;
		Ops[0x62] = &SetBoneOp;
		Ops[0x63] = &InlineKeyEvents;
		Ops[0x64] = &SetWind;
		Ops[0x6B] = &SetRotationOrder;
	}
}
