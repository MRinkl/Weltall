#include "GearEngineSteps.h"

#include "Graphics/GteVectors.h"

namespace Weltall::GearMachine
{
	namespace
	{
		constexpr uint32 Running = 0x100;
		constexpr uint32 Ended = 0x200;
		constexpr uint32 Looped = 0x400;
		constexpr uint32 WatchedRunning = 1;
		constexpr uint32 WatchedEnded = 2;
		constexpr uint32 WatchedLooped = 4;
		constexpr int8 PackedValueFollows = -0x80;
		constexpr int16 TurnTicksLimit = 32000;
		constexpr std::array<uint8, 3> AxisAbsent = {GearChannelKind::AbsentX, GearChannelKind::AbsentY, GearChannelKind::AbsentZ};

		enum class EValue
		{
			Rotation,
			Translation,
			Scale,
		};

		/** A stream's next value for Axis: its 16 bits, or a packed delta (added to Current) or the value it flags. */
		int32 ReadStream(FGearEngine& Engine, FGearChannel& Channel, int32 Current, bool bPacked)
		{
			FGearCodeRef& Read = Channel.StreamRead;
			if (!bPacked)
			{
				const int16 Value = ReadS16(Engine, Read);
				Read.Offset += 2;
				return Value;
			}
			const int8 Delta = static_cast<int8>(ReadU8(Engine, Read));
			Read.Offset += 1;
			if (Delta != PackedValueFollows)
			{
				return WrapAdd(Current, Delta);
			}
			const int16 Value = ReadS16(Engine, Read);
			Read.Offset += 2;
			return Value;
		}

		/** The kinds 3-5 of any value; Values are the bone's, read and written as ints. */
		void StepMove(FGearEngine& Engine, FGearChannel& Channel, EValue What, std::array<int32, 3>& Values)
		{
			std::array<int16, 3> From = {Channel.Words[0], Channel.Words[1], Channel.Words[2]};
			const std::array<int16, 3> To = {Channel.Words[3], Channel.Words[4], Channel.Words[5]};
			switch (Channel.KindNumber())
			{
			case GearChannelKind::Linear:
			{
				const int32 Tick = static_cast<int16>(Channel.Tick + 1);
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					const int32 Along = Divide(Engine, To[Axis] * Tick, Channel.Length);
					Values[Axis] = What == EValue::Translation ? WrapAdd(From[Axis], Along) : static_cast<int16>(From[Axis] + static_cast<int16>(Along));
				}
				break;
			}
			case GearChannelKind::Approach:
			{
				std::array<int32, 3> Steps{};
				bool bArrived = true;
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					const int32 Current = What == EValue::Scale ? From[Axis] : Values[Axis];
					Steps[Axis] = Divide(Engine, static_cast<int32>(static_cast<uint32>(To[Axis]) - static_cast<uint32>(Current)), Channel.Length);
					bArrived = bArrived && (Steps[Axis] & 0xFFFF) == 0;
				}
				if (bArrived)
				{
					Channel.Tick = Channel.Length;
					Values = {To[0], To[1], To[2]};
					break;
				}
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					if (What == EValue::Scale)
					{
						From[Axis] = static_cast<int16>(From[Axis] + static_cast<int16>(Steps[Axis]));
						Values[Axis] = From[Axis];
					}
					else
					{
						Values[Axis] = What == EValue::Translation ? WrapAdd(Values[Axis], static_cast<int16>(Steps[Axis])) : static_cast<int16>(Values[Axis] + static_cast<int16>(Steps[Axis]));
					}
				}
				Channel.Tick = 0;
				break;
			}
			case GearChannelKind::Accelerate:
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					From[Axis] = static_cast<int16>(From[Axis] + To[Axis]);
					Values[Axis] = What == EValue::Translation ? WrapAdd(Values[Axis], From[Axis]) : static_cast<int16>(Values[Axis] + From[Axis]);
				}
				break;
			default:
				break;
			}
			Channel.Words[0] = From[0];
			Channel.Words[1] = From[1];
			Channel.Words[2] = From[2];
		}

		/** The end of every kind's step but the turns: counted, and ended, looped or running as the events say. */
		void EndStep(FGearEngine& Engine, int16& Index, uint32 Watch, uint32& Events, bool bRestartsStreams)
		{
			FGearChannel& Channel = Engine.Channels[Index];
			Channel.Tick = static_cast<int16>(Channel.Tick + 1);
			const bool bWatched = Channel.Tag == Watch;
			if (Channel.Tick < Channel.Length)
			{
				Events |= Running | (bWatched ? WatchedRunning : 0);
				return;
			}
			if (Channel.Loop == 0)
			{
				Events |= Ended | (bWatched ? WatchedEnded : 0);
				FreeChannel(Engine, Index);
				return;
			}
			Events |= Looped | (bWatched ? WatchedLooped : 0);
			if (bRestartsStreams && Channel.IsStream())
			{
				Channel.Tick = 0;
				Channel.StreamRead = Channel.StreamStart;
				return;
			}
			// The tick after the loop's is 0: a move starts again; a looping acceleration keeps its speed.
			Channel.Tick = -1;
			if (Channel.KindNumber() == GearChannelKind::Accelerate)
			{
				Channel.Words[3] = Channel.Words[4] = Channel.Words[5] = 0;
			}
		}

		/** A turn of the bone's yaw (and pitch) toward the channel's point, a step that grows as it goes. */
		void StepTurn(FGearEngine& Engine, FGearChannel& Channel, FGearBone& Bone)
		{
			const int32 X = Channel.Words[3] - Bone.Position[0];
			const int32 Y = Channel.Words[4] - Bone.Position[1];
			const int32 Z = Channel.Words[5] - Bone.Position[2];
			const int32 Far = Distance(Engine, X, Y, Z);
			const int32 Step = Channel.Words[1] + Divide(Engine, WrapMul(Far + 1 + Channel.Tick, Channel.Words[2]), Channel.Words[0]);
			const auto Turn = [Step](int16& Angle, int32 Goal) {
				int32 Error = static_cast<int32>(static_cast<uint32>(Goal - Angle) & 0xFFF);
				if (Error > 0x7FF)
				{
					Error -= 0x1000;
				}
				const int32 Size = Error < 0 ? -Error : Error;
				if (Step > Size)
				{
					Angle = static_cast<int16>(Angle + Error);
				}
				else
				{
					Angle = static_cast<int16>(Error < 0 ? Angle - static_cast<int16>(Step) : Angle + static_cast<int16>(Step));
				}
			};
			Turn(Bone.Angles[1], Ratan2(Engine, -X, -Z));
			if (Channel.KindNumber() == GearChannelKind::TurnToward)
			{
				Turn(Bone.Angles[0], Ratan2(Engine, Y, Distance(Engine, X, 0, Z)));
			}
			if (Channel.Tick < TurnTicksLimit)
			{
				Channel.Tick = static_cast<int16>(Channel.Tick + Channel.Length);
			}
		}

		void StepRotation(FGearEngine& Engine, FGearBone& Bone, uint32 Watch, uint32& Events)
		{
			FGearChannel& Channel = Engine.Channels[Bone.Rotation];
			const uint8 Kind = Channel.KindNumber();
			if (Kind == GearChannelKind::TurnToward || Kind == GearChannelKind::TurnTowardYaw)
			{
				StepTurn(Engine, Channel, Bone);
			}
			else
			{
				if (Kind <= GearChannelKind::Deltas)
				{
					for (std::size_t Axis = 0; Axis < 3; ++Axis)
					{
						if ((Channel.Kind & AxisAbsent[Axis]) == 0)
						{
							const int32 Value = ReadStream(Engine, Channel, Bone.Angles[Axis], Kind == GearChannelKind::PackedDeltas);
							Bone.Angles[Axis] = static_cast<int16>(Kind == GearChannelKind::Deltas ? Bone.Angles[Axis] + Value : Value);
						}
					}
				}
				else
				{
					std::array<int32, 3> Angles = {Bone.Angles[0], Bone.Angles[1], Bone.Angles[2]};
					StepMove(Engine, Channel, EValue::Rotation, Angles);
					Bone.Angles = {static_cast<int16>(Angles[0]), static_cast<int16>(Angles[1]), static_cast<int16>(Angles[2])};
				}
				EndStep(Engine, Bone.Rotation, Watch, Events, true);
			}
			Bone.bRotationDirty = 1;
			Bone.bWorldDirty = 1;
		}

		void StepTranslation(FGearEngine& Engine, FGearBone& Bone, int32 ModelScale, uint32 Watch, uint32& Events)
		{
			FGearChannel& Channel = Engine.Channels[Bone.Translation];
			const uint8 Kind = Channel.KindNumber();
			if (Kind == GearChannelKind::Deltas)
			{
				// A step in the bone's frame, scaled by the bone and the model: the root's walk.
				FGteVector Step{};
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					const int32 Value = (Channel.Kind & AxisAbsent[Axis]) == 0 ? ReadStream(Engine, Channel, 0, false) : 0;
					Step[Axis] = static_cast<int16>((Value * Bone.Scale[Axis]) >> 12);
				}
				const FGteLongVector Moved = ApplyMatrix(Bone.World, Step);
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Bone.Position[Axis] = WrapAdd(Bone.Position[Axis], WrapMul(ModelScale, Moved[Axis]) >> 12);
				}
			}
			else if (Kind <= GearChannelKind::PackedDeltas)
			{
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					if ((Channel.Kind & AxisAbsent[Axis]) == 0)
					{
						Bone.Position[Axis] = ReadStream(Engine, Channel, Bone.Position[Axis], Kind == GearChannelKind::PackedDeltas);
					}
				}
			}
			else
			{
				StepMove(Engine, Channel, EValue::Translation, Bone.Position);
			}
			EndStep(Engine, Bone.Translation, Watch, Events, true);
			Bone.bWorldDirty = 1;
		}

		void StepScale(FGearEngine& Engine, FGearBone& Bone, uint32 Watch, uint32& Events)
		{
			FGearChannel& Channel = Engine.Channels[Bone.Scaling];
			std::array<int32, 3> Scale = {Bone.Scale[0], Bone.Scale[1], Bone.Scale[2]};
			const uint8 Kind = Channel.KindNumber();
			if (Kind >= GearChannelKind::Linear && Kind <= GearChannelKind::Accelerate)
			{
				StepMove(Engine, Channel, EValue::Scale, Scale);
				Bone.Scale = {static_cast<int16>(Scale[0]), static_cast<int16>(Scale[1]), static_cast<int16>(Scale[2])};
			}
			EndStep(Engine, Bone.Scaling, Watch, Events, false);
			Bone.bRotationDirty = 1;
			Bone.bWorldDirty = 1;
		}
	}

	uint32 StepChannels(FGearEngine& Engine, FGearBody& Body)
	{
		uint32 Events = 0;
		const uint32 Watch = Body.WatchTag;
		for (std::size_t Index = 0; Index < Body.Bones[0].Index && !Engine.Faulted(); ++Index)
		{
			FGearBone& Bone = Body.Bones[Index];
			if (Bone.Rotation != FGearBone::NoChannel)
			{
				StepRotation(Engine, Bone, Watch, Events);
			}
			if (Bone.Translation != FGearBone::NoChannel)
			{
				StepTranslation(Engine, Bone, Body.Scale, Watch, Events);
			}
			if (Bone.Scaling != FGearBone::NoChannel)
			{
				StepScale(Engine, Bone, Watch, Events);
			}
		}
		return Events;
	}
}
