#include "GearEngineSteps.h"

#include <algorithm>

namespace Weltall::GearMachine
{
	namespace
	{
		constexpr uint32 NoStream = 0xFFFF;
		constexpr uint32 WithCurrentFrom = 0x20;
		constexpr uint32 WithCurrentTo = 0x40;
		constexpr uint32 WithChildren = 0x80;

		/** A rotation's way from one angle to another, the short way round (4096 a turn). */
		int16 AngleDelta(int32 To, int32 From)
		{
			const uint32 Delta = static_cast<uint32>(To - From) & 0xFFF;
			return static_cast<int16>(Delta > 0x7FF ? static_cast<int32>(Delta) - 0x1000 : static_cast<int32>(Delta));
		}

		bool IsLocked(const FGearEngine& Engine, int16 Channel)
		{
			return Channel != FGearBone::NoChannel && Engine.Channels[Channel].Tag == GearChannelTag::Locked;
		}

		void FreeUnless(FGearEngine& Engine, int16& Channel, bool (*Keep)(const FGearChannel&, uint8), uint8 Tag)
		{
			if (Channel != FGearBone::NoChannel && !Keep(Engine.Channels[Channel], Tag))
			{
				FreeChannel(Engine, Channel);
			}
		}

		/** A record's channel: the bone's own, or a new one; none where the bone's is locked or the pool is full. */
		int16 ChannelFor(FGearEngine& Engine, int16 Current)
		{
			if (Current == FGearBone::NoChannel)
			{
				return AllocateChannel(Engine);
			}
			return IsLocked(Engine, Current) ? FGearBone::NoChannel : Current;
		}

		void StartStream(FGearChannel& Channel, uint32 Loop, uint8 Tag, uint8 Kind, int16 Length, const FGearCodeRef& Stream)
		{
			Channel.bUsed = true;
			Channel.Loop = static_cast<uint8>(Loop & 1);
			Channel.Tag = Tag;
			Channel.Kind = Kind;
			Channel.Tick = 0;
			Channel.Length = Length;
			Channel.StreamStart = Stream;
			Channel.StreamRead = Stream;
		}

		void Blend(FGearChannel& Channel, uint32 Kind, uint32 Loop, uint8 Tag, int16 Ticks)
		{
			Channel.bUsed = true;
			Channel.Kind = static_cast<uint8>(GearChannelKind::Linear + (Kind & 1));
			Channel.Loop = static_cast<uint8>(Loop);
			Channel.Tag = Tag;
			Channel.Tick = 0;
			Channel.Length = Ticks;
			Channel.StreamStart = {};
			Channel.StreamRead = {};
		}
	}

	int16 AllocateChannel(FGearEngine& Engine)
	{
		return static_cast<int16>(Engine.Channels.Allocate());
	}

	void FreeChannel(FGearEngine& Engine, int16& Channel)
	{
		if (Channel != FGearBone::NoChannel)
		{
			Engine.Channels.Free(Channel);
			Channel = FGearBone::NoChannel;
		}
	}

	void FreeUnlockedChannels(FGearEngine& Engine, FGearBody& Body)
	{
		const auto Locked = [](const FGearChannel& Channel, uint8) { return Channel.Tag == GearChannelTag::Locked; };
		for (std::size_t Index = 0; Index < Body.Bones[0].Index; ++Index)
		{
			FGearBone& Bone = Body.Bones[Index];
			FreeUnless(Engine, Bone.Rotation, Locked, 0);
			FreeUnless(Engine, Bone.Translation, Locked, 0);
			FreeUnless(Engine, Bone.Scaling, Locked, 0);
		}
	}

	void FreeTaggedChannels(FGearEngine& Engine, FGearBody& Body, uint8 Tag)
	{
		const auto Other = [](const FGearChannel& Channel, uint8 Wanted) { return Channel.Tag != Wanted; };
		for (std::size_t Index = 0; Index < Body.Bones[0].Index; ++Index)
		{
			FGearBone& Bone = Body.Bones[Index];
			FreeUnless(Engine, Bone.Rotation, Other, Tag);
			FreeUnless(Engine, Bone.Translation, Other, Tag);
			FreeUnless(Engine, Bone.Scaling, Other, Tag);
		}
	}

	void FreeBoneChannels(FGearEngine& Engine, FGearBody& Body, int32 Bone, uint32 Which)
	{
		if (Bone >= Body.Bones[0].Index)
		{
			return;
		}
		FGearBone& Target = Body.Bones[static_cast<std::size_t>(Bone)];
		if ((Which & 1) != 0)
		{
			FreeChannel(Engine, Target.Rotation);
		}
		if ((Which & 2) != 0)
		{
			FreeChannel(Engine, Target.Translation);
		}
		if ((Which & 4) != 0)
		{
			FreeChannel(Engine, Target.Scaling);
		}
	}

	void PlayRecord(FGearEngine& Engine, FGearBody& Body, const FGearRecord& Record, uint32 Loop, uint8 Tag)
	{
		if (Record.bPoseOnly)
		{
			FreeUnlockedChannels(Engine, Body);
			SnapPose(Engine, Body, Record);
			return;
		}
		const std::size_t Count = std::min<std::size_t>(Body.Bones[0].Index, Record.Rotations + 1u);
		const int16 Length = static_cast<int16>((Loop & 1) != 0 ? Record.Length : Record.Length - 1);
		const FGearCodeRef Descriptors = Record.At + static_cast<int32>(FGearRecord::HeaderBytes);
		uint32 Streams = FGearRecord::HeaderBytes + FGearRecord::DescriptorBytes * (Record.Rotations + 1u);
		if ((Record.Flags & FGearRecord::NoRotationPose) == 0)
		{
			Streams += FGearRecord::DescriptorBytes * Record.Rotations;
		}
		if ((Record.Flags & FGearRecord::NoTranslationPose) == 0)
		{
			Streams += FGearRecord::DescriptorBytes * Record.Translations;
		}
		for (std::size_t Index = 0; Index < Count && !Engine.Faulted(); ++Index)
		{
			const FGearCodeRef Descriptor = Descriptors + static_cast<int32>(FGearRecord::DescriptorBytes * Index);
			FGearBone& Bone = Body.Bones[Index];
			for (const bool bTranslation : {false, true})
			{
				int16& Current = bTranslation ? Bone.Translation : Bone.Rotation;
				const uint32 Stream = ReadU16(Engine, Descriptor + (bTranslation ? 2 : 0));
				if (Stream == NoStream)
				{
					if (Current != FGearBone::NoChannel && Index != 0 && !IsLocked(Engine, Current))
					{
						FreeChannel(Engine, Current);
					}
					continue;
				}
				const int16 Channel = ChannelFor(Engine, Current);
				if (Channel == FGearBone::NoChannel)
				{
					continue;
				}
				const uint8 Kind = ReadU8(Engine, Descriptor + (bTranslation ? 5 : 4));
				StartStream(Engine.Channels[Channel], Loop, Tag, Kind, Length, Record.At + static_cast<int32>(Streams + Stream));
				Current = Channel;
			}
		}
	}

	void SnapPose(FGearEngine& Engine, FGearBody& Body, const FGearRecord& Record)
	{
		FGearCodeRef Pose = Record.Pose();
		uint32 Rotations = 0;
		uint32 Translations = 0;
		for (std::size_t Index = 1; Index < Body.Bones[0].Index && !Engine.Faulted(); ++Index)
		{
			FGearBone& Bone = Body.Bones[Index];
			if ((Record.Flags & FGearRecord::NoRotationPose) == 0 && Rotations < Record.Rotations)
			{
				const FGteVector Angles = {ReadS16(Engine, Pose), ReadS16(Engine, Pose + 2), ReadS16(Engine, Pose + 4)};
				Pose = Pose + 6;
				++Rotations;
				if (Bone.Angles != Angles && !IsLocked(Engine, Bone.Rotation))
				{
					Bone.Angles = Angles;
					Bone.bWorldDirty = 1;
					Bone.bRotationDirty = 1;
				}
			}
			if ((Record.Flags & FGearRecord::NoTranslationPose) == 0 && Translations < Record.Translations)
			{
				const std::array<int32, 3> Position = {ReadS16(Engine, Pose), ReadS16(Engine, Pose + 2), ReadS16(Engine, Pose + 4)};
				Pose = Pose + 6;
				++Translations;
				if (Bone.Position != Position && !IsLocked(Engine, Bone.Translation))
				{
					Bone.Position = Position;
					Bone.bWorldDirty = 1;
				}
			}
		}
	}

	void BlendToPose(FGearEngine& Engine, FGearBody& Body, const FGearRecord& Record, int32 Ticks, uint32 Kind, uint32 Loop, uint8 Tag)
	{
		const int16 Length = static_cast<int16>(Ticks == 0 ? 1 : Ticks);
		const bool bTarget = (Kind & 1) != 0;
		Loop &= 1;
		FGearCodeRef Pose = Record.Pose();
		uint32 Rotations = 0;
		uint32 Translations = 0;
		for (std::size_t Index = 1; Index < Body.Bones[0].Index && !Engine.Faulted(); ++Index)
		{
			FGearBone& Bone = Body.Bones[Index];
			bool bBlend = false;
			std::array<int16, 3> Values{};
			if ((Record.Flags & FGearRecord::NoRotationPose) == 0 && Rotations < Record.Rotations)
			{
				Values = {ReadS16(Engine, Pose), ReadS16(Engine, Pose + 2), ReadS16(Engine, Pose + 4)};
				Pose = Pose + 6;
				++Rotations;
				bBlend = Bone.Angles != FGteVector{Values[0], Values[1], Values[2]};
			}
			if (bBlend && !IsLocked(Engine, Bone.Rotation))
			{
				const int16 Channel = ChannelFor(Engine, Bone.Rotation);
				if (Channel != FGearBone::NoChannel)
				{
					FGearChannel& Target = Engine.Channels[Channel];
					Blend(Target, Kind, Loop, Tag, Length);
					for (std::size_t Axis = 0; Axis < 3; ++Axis)
					{
						Target.Words[Axis] = Bone.Angles[Axis];
						const int16 Delta = AngleDelta(Values[Axis], Bone.Angles[Axis]);
						Target.Words[3 + Axis] = bTarget ? static_cast<int16>(Delta + Bone.Angles[Axis]) : Delta;
					}
					Bone.Rotation = Channel;
				}
			}
			else if (!bBlend && !IsLocked(Engine, Bone.Rotation))
			{
				FreeChannel(Engine, Bone.Rotation);
			}
			bBlend = false;
			if ((Record.Flags & FGearRecord::NoTranslationPose) == 0 && Translations < Record.Translations)
			{
				Values = {ReadS16(Engine, Pose), ReadS16(Engine, Pose + 2), ReadS16(Engine, Pose + 4)};
				Pose = Pose + 6;
				++Translations;
				bBlend = Bone.Position != std::array<int32, 3>{Values[0], Values[1], Values[2]};
			}
			if (bBlend && !IsLocked(Engine, Bone.Translation))
			{
				const int16 Channel = ChannelFor(Engine, Bone.Translation);
				if (Channel != FGearBone::NoChannel)
				{
					FGearChannel& Target = Engine.Channels[Channel];
					Blend(Target, Kind, Loop, Tag, Length);
					for (std::size_t Axis = 0; Axis < 3; ++Axis)
					{
						Target.Words[Axis] = static_cast<int16>(Bone.Position[Axis]);
						Target.Words[3 + Axis] = bTarget ? Values[Axis] : static_cast<int16>(Values[Axis] - static_cast<int16>(Bone.Position[Axis]));
					}
					Bone.Translation = Channel;
				}
			}
			else if (!bBlend && !IsLocked(Engine, Bone.Translation))
			{
				FreeChannel(Engine, Bone.Translation);
			}
		}
	}

	void RotateRootTo(FGearEngine& Engine, FGearBody& Body, int32 Ticks, int32 X, int32 Y, int32 Z)
	{
		FGearBone& Root = Body.Bones[0];
		if (Ticks < 2)
		{
			Root.Angles = {static_cast<int16>(X), static_cast<int16>(Y), static_cast<int16>(Z)};
			Root.bRotationDirty = 1;
			return;
		}
		if (Root.Angles[0] == X && Root.Angles[1] == Y && Root.Angles[2] == Z)
		{
			return;
		}
		const int16 Channel = Root.Rotation != FGearBone::NoChannel ? Root.Rotation : AllocateChannel(Engine);
		if (Channel == FGearBone::NoChannel)
		{
			return;
		}
		FGearChannel& Target = Engine.Channels[Channel];
		Blend(Target, 0, 0, GearChannelTag::RootTurns, static_cast<int16>(Ticks));
		const std::array<int32, 3> To = {X, Y, Z};
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			Target.Words[Axis] = Root.Angles[Axis];
			Target.Words[3 + Axis] = AngleDelta(To[Axis], Root.Angles[Axis]);
		}
		Root.Rotation = Channel;
	}

	void TurnRootToward(FGearEngine& Engine, FGearBody& Body, uint8 YawOnly, int16 Gain, int16 Speed, int16 Rate)
	{
		FGearBone& Root = Body.Bones[0];
		const int16 Channel = Root.Rotation != FGearBone::NoChannel ? Root.Rotation : AllocateChannel(Engine);
		if (Channel == FGearBone::NoChannel)
		{
			return;
		}
		FGearChannel& Target = Engine.Channels[Channel];
		Target.bUsed = true;
		Target.Kind = static_cast<uint8>(GearChannelKind::TurnToward + YawOnly);
		Target.Loop = 0;
		Target.Tag = GearChannelTag::RootTurns;
		const int32 Far = Distance(Engine, Body.Target[0] - Root.Position[0], Body.Target[1] - Root.Position[1], Body.Target[2] - Root.Position[2]);
		Target.Words = {static_cast<int16>(Far + 1), Gain, Speed, Body.Target[0], Body.Target[1], Body.Target[2]};
		Target.StreamStart = {};
		Target.StreamRead = {};
		Target.Tick = 0;
		Target.Length = Rate;
		Root.Rotation = Channel;
	}

	void SetChannel(FGearEngine& Engine, FGearBody& Body, int32 Bone, uint32 Flags, uint32 Kind, uint32 Tag, uint32 Loop, const std::array<int16, 6>& Words, int16 Ticks)
	{
		FGearBone* Target = BoneAt(Engine, Body, Bone);
		if (Target == nullptr)
		{
			return;
		}
		const uint32 Which = Flags & 7;
		int16& Current = Which == 0 ? Target->Rotation : Which == 1 ? Target->Translation : Target->Scaling;
		const int16 Channel = Current != FGearBone::NoChannel ? Current : AllocateChannel(Engine);
		if (Channel != FGearBone::NoChannel)
		{
			const auto Value = [&](std::size_t Axis) -> int16 {
				return Which == 0 ? Target->Angles[Axis] : Which == 1 ? static_cast<int16>(Target->Position[Axis]) : Target->Scale[Axis];
			};
			FGearChannel& Moving = Engine.Channels[Channel];
			Moving.bUsed = true;
			Moving.Kind = static_cast<uint8>(Kind + GearChannelKind::Linear);
			Moving.Loop = static_cast<uint8>(Loop);
			Moving.Tag = static_cast<uint8>(Tag);
			Moving.StreamStart = {};
			Moving.StreamRead = {};
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				const int16 From = static_cast<int16>(Words[Axis] + ((Flags & WithCurrentFrom) != 0 ? Value(Axis) : 0));
				const int16 To = static_cast<int16>(Words[3 + Axis] + ((Flags & WithCurrentTo) != 0 ? Value(Axis) : 0));
				Moving.Words[Axis] = From;
				Moving.Words[3 + Axis] = (Kind & 0xFF) == 0 ? static_cast<int16>(To - From) : To;
			}
			Moving.Tick = 0;
			Moving.Length = Ticks;
			if ((Kind & 0xFF) < 2)
			{
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					if (Which == 0)
					{
						Target->Angles[Axis] = Moving.Words[Axis];
					}
					else if (Which == 1)
					{
						Target->Position[Axis] = Moving.Words[Axis];
					}
					else
					{
						Target->Scale[Axis] = Moving.Words[Axis];
					}
				}
			}
			Current = Channel;
		}
		if ((Flags & WithChildren) == 0)
		{
			return;
		}
		for (std::size_t Child = 1; Child < Body.Bones[0].Index && !Engine.Faulted(); ++Child)
		{
			if (Body.Bones[Child].Parent == Bone)
			{
				SetChannel(Engine, Body, static_cast<int32>(Child), Flags & 0xFF, Kind & 0xFF, Tag & 0xFF, Loop & 0xFF, Words, Ticks);
			}
		}
	}

	void SetBone(FGearEngine& Engine, FGearBody& Body, int32 Bone, uint32 Flags, int16 X, int16 Y, int16 Z)
	{
		FGearBone* Target = BoneAt(Engine, Body, Bone);
		if (Target == nullptr)
		{
			return;
		}
		const bool bAdd = (Flags & WithCurrentFrom) != 0;
		const std::array<int16, 3> Values = {X, Y, Z};
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			switch (Flags & 7)
			{
			case 0:
				Target->Angles[Axis] = static_cast<int16>(Values[Axis] + (bAdd ? Target->Angles[Axis] : 0));
				break;
			case 1:
				Target->Position[Axis] = Values[Axis] + (bAdd ? Target->Position[Axis] : 0);
				break;
			default:
				Target->Scale[Axis] = static_cast<int16>(Values[Axis] + (bAdd ? Target->Scale[Axis] : 0));
				break;
			}
		}
		Target->bWorldDirty = 1;
		Target->bRotationDirty = 1;
		if ((Flags & WithChildren) == 0)
		{
			return;
		}
		for (std::size_t Child = 1; Child < Body.Bones[0].Index && !Engine.Faulted(); ++Child)
		{
			if (Body.Bones[Child].Parent == Bone)
			{
				SetBone(Engine, Body, static_cast<int32>(Child), Flags & 0xFF, X, Y, Z);
			}
		}
	}

	void ShowBone(FGearEngine& Engine, FGearBody& Body, int32 Bone, uint32 Flags)
	{
		FGearBone* Target = BoneAt(Engine, Body, Bone);
		if (Target == nullptr)
		{
			return;
		}
		Target->bShown = static_cast<uint8>(Flags & 1);
		if ((Flags & WithChildren) == 0)
		{
			return;
		}
		for (std::size_t Child = 1; Child < Body.Bones[0].Index && !Engine.Faulted(); ++Child)
		{
			if (Body.Bones[Child].Parent == Bone)
			{
				ShowBone(Engine, Body, static_cast<int32>(Child), Flags);
			}
		}
	}
}
