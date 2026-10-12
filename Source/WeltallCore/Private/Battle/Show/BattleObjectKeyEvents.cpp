#include "BattleObjectMachine.h"

#include "Battle/Show/BattleShowFloor.h"
#include "Battle/Show/BattleStageFiles.h"
#include "Graphics/GteVectors.h"

#include <string>

namespace Weltall::BattleObjectMachine
{
	namespace
	{
		using GearMachine::ReadS16;
		using GearMachine::ReadU8;

		constexpr uint8 FromGearTable = 0x80;
		constexpr uint8 AtFirstTarget = 0x80;
		constexpr uint8 OnFloor = 1;
		constexpr uint8 OnGround = 2;
		constexpr int16 QuarterTurn = 0x400;
		constexpr uint8 GroundLanding = 3;
		constexpr uint8 FootstepSound = 1;
		constexpr uint8 LandingSound = 2;
		constexpr uint32 FootstepBase = 10;
		constexpr uint32 LandingShift = 8;
		constexpr int32 LandingVolume = 0x3C;
		constexpr int32 FullVolume = 0x6B;
		constexpr int32 MaskedSlots = 13;
		constexpr uint32 EveryBuffer = 0x700;
		constexpr uint8 NoFeed = 0xFF;
		constexpr uint8 RelativeClut = 0x80;
		constexpr uint32 FromVram = 1;

		/** Each kind's record length; an unknown kind's record is not passed over. */
		constexpr int32 EffectBytes = 0x14;
		constexpr int32 LightBytes = 0x12;
		constexpr int32 OffBytes = 6;
		constexpr int32 TrailBytes = 0x1C;
		constexpr int32 SoundBytes = 8;
		constexpr int32 NumbersBytes = 4;
		constexpr int32 BoneBytes = 6;
		constexpr int32 ReactionBytes = 10;
		constexpr int32 ClutBytes = 0x1C;

		std::optional<int32> SpawnEffect(FBattleObjects& Objects, FBattleObject& Object, const FGearCodeRef& Event)
		{
			const uint8 Number = ReadU8(Objects, Event + 3);
			const uint8 Filter = ReadU8(Objects, Event + 4);
			const uint8 Bone = ReadU8(Objects, Event + 5);
			const FGteVector Offset = {ReadS16(Objects, Event + 6), ReadS16(Objects, Event + 8), ReadS16(Objects, Event + 10)};
			const uint8 Place = ReadU8(Objects, Event + 12);
			const uint8 Absolute = ReadU8(Objects, Event + 13);
			const int16 Direction = ReadS16(Objects, Event + 14);
			const int16 Scale = ReadS16(Objects, Event + 16);
			const uint8 Resource = ReadU8(Objects, Event + 18);
			const uint8 Follows = ReadU8(Objects, Event + 19);
			uint16 Animation = Number;
			if ((Filter & FromGearTable) != 0)
			{
				const std::optional<uint8> Base = GearEffectBase(Objects, Object.Slot, Number);
				if (!Base)
				{
					return std::nullopt;
				}
				Animation = static_cast<uint16>(Number + (*Base - 1));
			}
			const FBattleObject* Placer = &Object;
			int32 RecordSlot = 0;
			if ((Place & AtFirstTarget) != 0 || (Filter & FromGearTable) != 0)
			{
				const uint32 Target = FirstTarget(Objects);
				Placer = Objects.Object(static_cast<int32>(Target));
				if (Placer == nullptr)
				{
					RecordSlot = static_cast<int32>(Target) + 1;
					Placer = &Object;
				}
			}
			if (static_cast<int16>(Animation) < 0)
			{
				return EffectBytes;
			}
			FGteMatrix Frame = Placer->Bones[0].Local;
			// The original reads a bone the placer lacks past its skeleton, from whatever the heap holds there:
			// such an effect is placed at the placer's root.
			if (Bone != 0 && Bone < Placer->Bones.size())
			{
				Frame = CompMatrix(Placer->Bones[0].Local, Placer->Bones[Bone].World);
			}
			if (RecordSlot != 0)
			{
				const std::optional<std::array<int16, 3>> At = SlotPlace(Objects, RecordSlot - 1);
				if (!At)
				{
					return std::nullopt;
				}
				// The record's words go in unsigned.
				Frame.T = {static_cast<uint16>((*At)[0]), static_cast<uint16>((*At)[1]), static_cast<uint16>((*At)[2])};
			}
			const FGteLongVector Placed = RotTrans(Frame, Offset);
			FGteVector Point = {static_cast<int16>(Placed[0]), static_cast<int16>(Placed[1]), static_cast<int16>(Placed[2])};
			if ((Place & 0x7F) == OnFloor)
			{
				Point[1] = Object.FloorY;
			}
			else if ((Place & 0x7F) == OnGround)
			{
				Point = Objects.Stage->GroundPoint();
				Objects.Floor->Height(Point, Objects.Floor->ScanTriangles(Point));
			}
			FBattleBoneEffect Effect;
			Effect.bEffectResource = Resource != 0;
			Effect.Animation = static_cast<int16>(Animation);
			Effect.Point = Point;
			Effect.Direction = Absolute == 0 ? static_cast<int16>(Object.Bones[0].Angles[1] + Direction + QuarterTurn) : Direction;
			Effect.Scale = static_cast<int16>(static_cast<uint32>(Scale * Object.Scale) >> 8);
			Effect.Event = Event;
			Effect.Slot = static_cast<uint8>(Objects.PlaceOf(Object));
			Effect.Bone = Bone;
			Effect.bFollows = Follows != 0;
			Effect.FollowOffset = Offset;
			Effect.bOnFloor = Place != 0;
			if (ResultMatches(Objects, static_cast<int32>(FirstTarget(Objects)), Filter))
			{
				Objects.Services->SpawnEffect(Effect);
			}
			return EffectBytes;
		}

		std::optional<int32> PointLight(FBattleObjects& Objects, FBattleObject& Object, const FGearCodeRef& Event)
		{
			const uint8 Index = ReadU8(Objects, Event + 3);
			if (ReadU8(Objects, Event + 4) == 0)
			{
				if (Index >= Objects.PointLights.size())
				{
					Objects.Fail("key event 2 puts out point light " + std::to_string(Index) + ", past the two");
					return std::nullopt;
				}
				Objects.PointLights[Index].Strength = 0;
				return OffBytes;
			}
			if (Index < Objects.PointLights.size())
			{
				FBattlePointLight& Lamp = Objects.PointLights[Index];
				Lamp.Slot = ReadU8(Objects, Event + 5) == 0 ? static_cast<int16>(Object.Slot) : int16{-1};
				Lamp.Bone = ReadU8(Objects, Event + 6);
				for (std::size_t Row = 0; Row < 3; ++Row)
				{
					Objects.ColourMatrix.M[Row][Index + 1] = static_cast<int16>(ReadU8(Objects, Event + 7 + static_cast<int32>(Row)) << 4);
				}
				Lamp.Offset = {ReadS16(Objects, Event + 10), ReadS16(Objects, Event + 12), ReadS16(Objects, Event + 14)};
				Lamp.Strength = ReadS16(Objects, Event + 16);
			}
			return LightBytes;
		}

		std::optional<int32> Trail(FBattleObjects& Objects, FBattleObject& Object, const FGearCodeRef& Event, uint8 Kind)
		{
			const uint8 Index = ReadU8(Objects, Event + 3);
			// The original stops an emitter past the object's own unchecked, writing into whatever block follows them, and
			// starts none there.
			if (Index >= Object.Trails.size())
			{
				return ReadU8(Objects, Event + 4) == 0 ? OffBytes : TrailBytes;
			}
			FBattleTrailEmitter& Emitter = Object.Trails[Index];
			Emitter.Bone = -1;
			if (ReadU8(Objects, Event + 4) == 0)
			{
				return OffBytes;
			}
			const int16 Life = ReadU8(Objects, Event + 7);
			if (Life == 0)
			{
				Objects.Fail("a trail starts with no life, which its fade divides by");
				return std::nullopt;
			}
			Emitter.Blend = ReadU8(Objects, Event + 0x1A);
			Emitter.Counter = -1;
			Emitter.Bone = ReadU8(Objects, Event + 5);
			Emitter.Kind = static_cast<uint8>(Kind - 3);
			Emitter.Points[0] = {ReadS16(Objects, Event + 0xE), ReadS16(Objects, Event + 0x10), ReadS16(Objects, Event + 0x12)};
			Emitter.Points[1] = {ReadS16(Objects, Event + 0x14), ReadS16(Objects, Event + 0x16), ReadS16(Objects, Event + 0x18)};
			Emitter.Ring = 0;
			Emitter.SegmentSamples = std::min<int16>(ReadU8(Objects, Event + 6), 7);
			Emitter.Life = Life;
			Emitter.Segment = -1;
			for (std::size_t Channel = 0; Channel < 3; ++Channel)
			{
				Emitter.Colour[Channel] = static_cast<int16>(ReadU8(Objects, Event + 8 + static_cast<int32>(Channel)) << 6);
				const int32 Faded = Emitter.Colour[Channel] - ReadU8(Objects, Event + 11 + static_cast<int32>(Channel)) * 0x40;
				Emitter.Fade[Channel] = static_cast<int16>(Faded / Life);
			}
			return TrailBytes;
		}

		std::optional<int32> Sound(FBattleObjects& Objects, FBattleObject& Object, const FGearCodeRef& Event)
		{
			const uint8 First = ReadU8(Objects, Event + 3);
			const uint8 Filter = ReadU8(Objects, Event + 4);
			const uint8 Base = ReadU8(Objects, Event + 5);
			const uint8 Second = ReadU8(Objects, Event + 6);
			const uint8 Kind = ReadU8(Objects, Event + 7);
			if (!ResultMatches(Objects, static_cast<int32>(FirstTarget(Objects)), Filter) || Objects.Faulted())
			{
				return SoundBytes;
			}
			const uint8 Ground = Objects.Stage->GroundSound();
			if (Kind == FootstepSound && Ground != 0)
			{
				const std::optional<uint32> Own = SoundBase(Objects, Object, 1);
				if (!Own)
				{
					return std::nullopt;
				}
				Objects.Services->PlaySound(*Own | (Ground + FootstepBase));
			}
			const std::optional<uint32> Set = SoundBase(Objects, Object, Base);
			if (!Set)
			{
				return std::nullopt;
			}
			const uint32 Extra = Ground == GroundLanding && Kind == LandingSound ? LandingShift : 0;
			Objects.Services->PlaySound(*Set + First + Extra);
			if (Second != 0)
			{
				Objects.Services->PlaySound(*Set + Second + Extra);
			}
			int32 Volume = Object.Loudness;
			if (Ground == GroundLanding && Kind != LandingSound)
			{
				Volume = Volume * LandingVolume / FullVolume;
			}
			Objects.Services->SetSoundVolume(*Set + First + Extra, static_cast<int16>(Volume));
			if (Second != 0)
			{
				Objects.Services->SetSoundVolume(*Set + Second + Extra, static_cast<int16>(Volume));
			}
			return SoundBytes;
		}

		std::optional<int32> ShowBone(FBattleObjects& Objects, FBattleObject& Object, const FGearCodeRef& Event)
		{
			const uint8 Bone = ReadU8(Objects, Event + 4);
			if (Bone >= Object.Bones.size())
			{
				Objects.Fail("key event 7 shows bone " + std::to_string(Bone) + ", which its object does not have");
				return std::nullopt;
			}
			Object.Bones[Bone].bShown = ReadU8(Objects, Event + 5) & 1;
			return BoneBytes;
		}

		std::optional<int32> Reactions(FBattleObjects& Objects, FBattleObject& Object, const FGearCodeRef& Event)
		{
			const uint16 SavedTargets = Objects.PlayTargets;
			const uint16 SavedPerformer = Objects.Performer;
			const uint16 From = static_cast<uint16>(1u << (static_cast<int16>(Objects.Performer) & 0x1F));
			const uint8 Filter = ReadU8(Objects, Event + 3);
			const uint8 FromOwner = ReadU8(Objects, Event + 4);
			const int32 Slot = Objects.PlaceOf(Object);
			for (int32 Target = 0; Target < MaskedSlots && !Objects.Faulted(); ++Target)
			{
				const FBattleObject* Self = Objects.Object(Slot);
				if (Self == nullptr)
				{
					Objects.Fail("an object is freed while its key event starts its targets' reactions");
					return std::nullopt;
				}
				if (((Self->Targets >> Target) & 1) == 0)
				{
					continue;
				}
				const FBattleObject* Reacting = Objects.Object(Target);
				const bool bReacts = Reacting != nullptr && (Reacting->Flags & BattleObjectFlags::Reacts) != 0;
				const uint8 OwnSequence = bReacts ? 0 : FromOwner;
				const std::optional<uint8> Result = ResultClass(Objects, Target);
				if (!Result)
				{
					return std::nullopt;
				}
				int32 Sequence = -1;
				switch (*Result)
				{
				case 0:
				case 1:
					Sequence = ReadU8(Objects, Event + (bReacts ? 6 : 5));
					break;
				case 2:
				case 3:
					Sequence = ReadU8(Objects, Event + 9);
					break;
				case 4:
					Sequence = ReadU8(Objects, Event + 8);
					break;
				case 5:
					Sequence = ReadU8(Objects, Event + 7);
					break;
				default:
					break;
				}
				if (!ResultMatches(Objects, Target, Filter))
				{
					Sequence = -1;
				}
				if (Reacting == nullptr || Sequence <= 0)
				{
					continue;
				}
				if (OwnSequence == 0)
				{
					Objects.React(Target, From, Sequence);
				}
				else
				{
					Objects.ReactFrom(Slot, Target, From, Sequence);
				}
			}
			Objects.Performer = SavedPerformer;
			Objects.PlayTargets = SavedTargets;
			return ReactionBytes;
		}

		/** Key event 9: a CLUT animation started (its place relative to the object's CLUT with bit 7 of its type) or stopped. */
		std::optional<int32> Clut(FBattleObjects& Objects, FBattleObject& Object, const FGearCodeRef& Event)
		{
			const uint8 Index = ReadU8(Objects, Event + 3);
			const int32 Count = static_cast<int32>(Object.ClutAnimations.size());
			if (ReadU8(Objects, Event + 4) == 0)
			{
				if (Index >= Count)
				{
					Objects.Fail("key event 9 stops CLUT animation " + std::to_string(Index) + ", past its object's");
					return std::nullopt;
				}
				StopClutAnimation(Objects, Object.ClutAnimations[Index]);
				return OffBytes;
			}
			if (Index >= Count)
			{
				return ClutBytes;
			}
			const uint8 Fed = ReadU8(Objects, Event + 5);
			const uint8 Type = ReadU8(Objects, Event + 6);
			const uint8 Sources = ReadU8(Objects, Event + 0x12);
			FBattleClutStart Start;
			Start.Type = Type & 0x7F;
			Start.Flags = Sources | EveryBuffer;
			Start.Feeds = Fed == NoFeed || Fed >= Count ? int16{-1} : static_cast<int16>(Fed);
			Start.bColourMatrix = Start.Type > 3;
			Start.Curve = ClutCurve(ReadU8(Objects, Event + 7));
			int16 FirstX = ReadS16(Objects, Event + 8);
			int16 FirstY = ReadS16(Objects, Event + 10);
			int16 SecondX = ReadS16(Objects, Event + 12);
			int16 SecondY = ReadS16(Objects, Event + 14);
			if ((Type & RelativeClut) != 0)
			{
				if (Object.TextureX < 0)
				{
					// The record is not passed over: the next events read it again.
					return 0;
				}
				FirstX = static_cast<int16>(FirstX + Object.ClutX);
				FirstY = static_cast<int16>(FirstY + Object.ClutY);
				if ((Sources >> 4) == FromVram)
				{
					SecondX = static_cast<int16>(SecondX + Object.ClutX);
					SecondY = static_cast<int16>(SecondY + Object.ClutY);
				}
			}
			Start.Values = {static_cast<uint16>(FirstX), static_cast<uint16>(FirstY), 0, static_cast<uint16>(SecondX), static_cast<uint16>(SecondY),
				static_cast<uint16>(ReadS16(Objects, Event + 16))};
			Start.X = FirstX;
			Start.Y = FirstY;
			Start.Width = ReadU8(Objects, Event + 0x13);
			Start.Height = ReadU8(Objects, Event + 0x14);
			Start.Speed = ReadS16(Objects, Event + 0x16);
			Start.Divisor = ReadS16(Objects, Event + 0x18);
			Start.Base = ReadS16(Objects, Event + 0x1A);
			StartClutAnimation(Objects, Object.ClutAnimations[static_cast<std::size_t>(Index)], Start);
			return ClutBytes;
		}
	}

	void RunKeyEvents(FBattleObjects& Objects, int32 Slot)
	{
		FBattleObject* Object = Objects.Object(Slot);
		if (Object == nullptr || Object->KeyFrame < 0)
		{
			return;
		}
		while (Object->KeyIndex < Object->KeyCount && !Objects.Faulted())
		{
			const FGearCodeRef Event = Object->KeyEvents;
			if (Object->KeyFrame != ReadS16(Objects, Event))
			{
				break;
			}
			const uint8 Kind = ReadU8(Objects, Event + 2);
			std::optional<int32> Length = 0;
			switch (Kind)
			{
			case 1:
				Length = SpawnEffect(Objects, *Object, Event);
				break;
			case 2:
				Length = PointLight(Objects, *Object, Event);
				break;
			case 3:
			case 4:
				Length = Trail(Objects, *Object, Event, Kind);
				break;
			case 5:
				Length = Sound(Objects, *Object, Event);
				break;
			case 6:
				Objects.Services->ShowNumbers();
				Length = NumbersBytes;
				break;
			case 7:
				Length = ShowBone(Objects, *Object, Event);
				break;
			case 8:
				Length = Reactions(Objects, *Object, Event);
				break;
			case 9:
				Length = Clut(Objects, *Object, Event);
				break;
			default:
				break;
			}
			if (!Length || Objects.Faulted())
			{
				return;
			}
			Object = Objects.Object(Slot);
			if (Object == nullptr)
			{
				Objects.Fail("an object is freed while its key events run");
				return;
			}
			Object->KeyEvents = Object->KeyEvents + *Length;
			++Object->KeyIndex;
		}
		Object->KeyFrame = static_cast<int16>(Object->KeyFrame + 1);
		if (Object->KeyLoop >= 0 && Object->KeyLoop <= Object->KeyFrame)
		{
			Object->KeyFrame = 0;
			Object->KeyIndex = 0;
			Object->KeyEvents = Object->KeyEventsLoop;
		}
	}
}
