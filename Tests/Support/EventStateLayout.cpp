#include "Support/EventStateLayout.h"

#include <type_traits>

namespace Weltall::Testing
{
	namespace
	{
		/** Calls Visit(offset, member) for every member at its offset in the original's layout. */
		template <typename FVisitor>
		void VisitLayout(FEventActorState& State, FVisitor&& Visit)
		{
			auto Array = [&Visit](std::size_t At, auto& Values)
			{
				for (std::size_t Index = 0; Index < Values.size(); ++Index)
				{
					Visit(At + Index * sizeof(Values[0]), Values[Index]);
				}
			};
			Visit(0x00, State.Flags0);
			Visit(0x04, State.Flags1);
			Array(0x08, State.FloorTriangles);
			Visit(0x10, State.Layer);
			Visit(0x12, State.Word12);
			Visit(0x14, State.FloorAttributes);
			Visit(0x18, State.HalfWidth);
			Visit(0x1A, State.Height);
			Visit(0x1C, State.HalfDepth);
			Visit(0x1E, State.Radius);
			Array(0x20, State.Position);
			Visit(0x2C, State.Word2C);
			Array(0x30, State.Step);
			Visit(0x3C, State.Word3C);
			Array(0x40, State.Velocity);
			Visit(0x4C, State.Word4C);
			Array(0x50, State.FloorNormal);
			Visit(0x5C, State.Word5C);
			Visit(0x60, State.InteractX);
			Visit(0x62, State.Word62);
			Visit(0x64, State.InteractZ);
			Visit(0x66, State.Word66);
			Array(0x68, State.PreviousPosition);
			Visit(0x6E, State.StillTicks);
			Visit(0x70, State.ExtraAngle);
			Visit(0x72, State.FloorHeight);
			Visit(0x74, State.LinkedActor);
			Visit(0x75, State.Parent);
			Visit(0x76, State.SpeedDivisor);
			Array(0x78, State.CallStack);
			Visit(0x80, State.Portrait);
			Visit(0x81, State.Choice);
			Visit(0x82, State.WindowWidth);
			Visit(0x83, State.WindowLines);
			Visit(0x84, State.WindowStyle);
			Visit(0x88, State.WindowX);
			Visit(0x8A, State.WindowY);
			for (std::size_t Slot = 0; Slot < State.Threads.size(); ++Slot)
			{
				FEventThread& Thread = State.Threads[Slot];
				Visit(0x8C + Slot * 8, Thread.Pc);
				Visit(0x8E + Slot * 8, Thread.WaitCount);
				Visit(0x8F + Slot * 8, Thread.Script);
				Visit(0x90 + Slot * 8, Thread.Word);
			}
			Visit(0xCC, State.Pc);
			Visit(0xCE, State.Slot);
			Visit(0xCF, State.RequestSlot);
			Array(0xD0, State.MoveWork);
			Array(0xDC, State.BytesDC);
			Visit(0xE0, State.JumpTicks);
			Visit(0xE2, State.DoorStep);
			Visit(0xE3, State.PushCount);
			Visit(0xE4, State.Character);
			Visit(0xE6, State.IdleAnimation);
			Visit(0xE8, State.Animation);
			Visit(0xEA, State.AnimationRequest);
			Visit(0xEC, State.TargetHeight);
			Visit(0xEE, State.DrawSize);
			Visit(0xF0, State.WordF0);
			Array(0xF4, State.Scale);
			Visit(0xFA, State.WordFA);
			Array(0xFC, State.Tints);
			Visit(0x102, State.Counter);
			Visit(0x104, State.MoveDirection);
			Visit(0x106, State.Facing);
			Visit(0x108, State.Yaw);
			Visit(0x10A, State.Sound);
			Visit(0x10C, State.SoundVolume);
			Visit(0x10D, State.SoundMode);
			Array(0x10E, State.Bytes10E);
			Visit(0x11C, State.LockedFacing);
			Visit(0x11E, State.TurnSpeed);
			Visit(0x124, State.ExternalAnimationFile);
			Visit(0x126, State.ModelId);
			Visit(0x127, State.ModelIndex);
			Visit(0x128, State.GearJoint);
			Visit(0x12A, State.Word12A);
			Visit(0x12C, State.Status);
			Visit(0x130, State.SpriteTexture);
			Visit(0x134, State.SpriteDraw);
		}

		/** Little-endian, whatever the host. */
		template <typename T>
		void Store(uint8* At, T Value)
		{
			using FUnsigned = std::make_unsigned_t<T>;
			const FUnsigned Bits = static_cast<FUnsigned>(Value);
			for (std::size_t Byte = 0; Byte < sizeof(T); ++Byte)
			{
				At[Byte] = static_cast<uint8>(Bits >> (8 * Byte));
			}
		}

		template <typename T>
		T Load(const uint8* At)
		{
			using FUnsigned = std::make_unsigned_t<T>;
			FUnsigned Bits = 0;
			for (std::size_t Byte = 0; Byte < sizeof(T); ++Byte)
			{
				Bits = static_cast<FUnsigned>(Bits | static_cast<FUnsigned>(At[Byte]) << (8 * Byte));
			}
			return static_cast<T>(Bits);
		}
	}

	std::array<uint8, EventStateBytes> ToOriginalLayout(const FEventActorState& State)
	{
		std::array<uint8, EventStateBytes> Bytes{};
		FEventActorState Copy = State;
		VisitLayout(Copy, [&Bytes](std::size_t At, auto& Member) { Store(Bytes.data() + At, Member); });
		return Bytes;
	}

	FEventActorState FromOriginalLayout(std::span<const uint8, EventStateBytes> Bytes)
	{
		FEventActorState State;
		VisitLayout(State, [&Bytes](std::size_t At, auto& Member) { Member = Load<std::remove_reference_t<decltype(Member)>>(Bytes.data() + At); });
		return State;
	}
}
