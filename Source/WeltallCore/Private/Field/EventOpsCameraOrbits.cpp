#include "EventOps.h"

#include "Field/FieldCamera.h"
#include "Graphics/Gte.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"

#include <array>

namespace Weltall::EventOps
{
	namespace
	{
		/** The scripted path's four points, as the scripts number them. */
		constexpr std::array<FFieldPoint FFieldCamera::*, 4> PathSlots = {&FFieldCamera::TargetFrom, &FFieldCamera::TargetTo, &FFieldCamera::EyeFrom,
			&FFieldCamera::EyeTo};
		/** The scripted camera's scale when F3 measures its path: 1. */
		constexpr int16 WholeScale = 0x1000;

		int32 Wrapped(uint32 Value)
		{
			return static_cast<int32>(Value);
		}

		/**
		 * The point a scripted camera orbit puts at Distance from a centre (16.16), Elevation degrees
		 * above it (the rise scaled by the scripted camera's scale), turned about it by Yaw.
		 */
		FFieldPoint OrbitPoint(const FFieldCamera& Camera, const FFieldPoint& Centre, int32 Yaw, int32 Elevation, int32 Distance)
		{
			const int32 Angle = (Elevation * 0xB60 >> 8) + 0xC00;
			auto Offset = [&Camera, Distance](int32 Trigonometric, int32 Sign)
			{
				const int32 Whole = Wrapped(static_cast<uint32>(Trigonometric) * static_cast<uint32>(Distance) * static_cast<uint32>(Sign * 0x20)) >> 16;
				return Wrapped(static_cast<uint32>(Whole) * static_cast<uint32>(static_cast<int32>(Camera.ScriptScale)) * 0x10u);
			};
			FFieldPoint Point = {Centre[0], Wrapped(static_cast<uint32>(Offset(GteCos(Angle), -1)) + static_cast<uint32>(Centre[1])),
				Wrapped(static_cast<uint32>(Offset(GteSin(Angle), 1)) + static_cast<uint32>(Centre[2]))};
			const FGteMatrix Turn = RotationXyz({0, static_cast<int16>(Yaw), 0});
			std::array<int32, 3> Across{};
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Across[Axis] = Wrapped(static_cast<uint32>(Centre[Axis]) - static_cast<uint32>(Point[Axis]));
			}
			const std::array<int32, 3> Turned = ApplyMatrixLV(Turn, Across);
			Point[0] = Wrapped(static_cast<uint32>(Turned[0]) + static_cast<uint32>(Centre[0]));
			Point[2] = Wrapped(static_cast<uint32>(Turned[2]) + static_cast<uint32>(Centre[2]));
			return Point;
		}

		/** A point's whole units into three variables: x, z, then y. */
		void SetPointVariables(FEventContext& Context, const FFieldPoint& Point, int32 At)
		{
			SetVar(Context, ReadU16(Context, At), static_cast<int16>(Point[0] >> 16));
			SetVar(Context, ReadU16(Context, At + 2), static_cast<int16>(Point[2] >> 16));
			SetVar(Context, ReadU16(Context, At + 4), static_cast<int16>(Point[1] >> 16));
		}

		/** One of the scripted path's four points; a slot past them is a fault (the original reads its stack). */
		const FFieldPoint* PathSlot(FEventContext& Context, uint8 Slot)
		{
			if (Slot >= PathSlots.size())
			{
				Fail(Context, EEventFault::TableIndexOutOfRange, Slot);
				return nullptr;
			}
			return &(Context.Camera.*PathSlots[Slot]);
		}

		/** EB: the point of an orbit about a centre the scripts give (x, z, y), its yaw, elevation and distance, into variables. */
		void OrbitAroundPoint(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 0xD);
			FFieldPoint Centre;
			Centre[0] = static_cast<int32>(static_cast<uint32>(ReadOperand(Context, 1, Flags, 0x80)) << 16);
			Centre[2] = static_cast<int32>(static_cast<uint32>(ReadOperand(Context, 3, Flags, 0x40)) << 16);
			Centre[1] = static_cast<int32>(static_cast<uint32>(ReadOperand(Context, 5, Flags, 0x20)) << 16);
			const int32 Yaw = ReadOperand(Context, 7, Flags, 0x10);
			const int32 Elevation = ReadOperand(Context, 9, Flags, 0x08);
			const int32 Distance = ReadOperand(Context, 0xB, Flags, 0x04);
			SetPointVariables(Context, OrbitPoint(Context.Camera, Centre, Yaw, Elevation, Distance), 0xE);
			Context.Run.Budget += 1;
			Advance(Context, 0x14);
		}

		/** EC: EB about one of the scripted path's points. */
		void OrbitAroundPathSlot(FEventContext& Context)
		{
			const FFieldPoint* Centre = PathSlot(Context, CodeByte(Context, 1));
			if (Centre == nullptr)
			{
				return;
			}
			const uint8 Flags = CodeByte(Context, 8);
			const int32 Yaw = ReadOperand(Context, 2, Flags, 0x80);
			const int32 Elevation = ReadOperand(Context, 4, Flags, 0x40);
			const int32 Distance = ReadOperand(Context, 6, Flags, 0x20);
			SetPointVariables(Context, OrbitPoint(Context.Camera, *Centre, Yaw, Elevation, Distance), 9);
			Context.Run.Budget += 1;
			Advance(Context, 0xF);
		}

		/** ED: one of the scripted path's points into variables. */
		void GetPathSlot(FEventContext& Context)
		{
			const FFieldPoint* Point = PathSlot(Context, CodeByte(Context, 1));
			if (Point == nullptr)
			{
				return;
			}
			SetPointVariables(Context, *Point, 2);
			Context.Run.Budget += 1;
			Advance(Context, 8);
		}

		/** EE: one of the scripted path's points copied to another; a destination past them copies nothing. */
		void CopyPathSlot(FEventContext& Context)
		{
			const uint8 To = CodeByte(Context, 2);
			if (To < PathSlots.size())
			{
				const FFieldPoint* From = PathSlot(Context, CodeByte(Context, 1));
				if (From == nullptr)
				{
					return;
				}
				Context.Camera.*PathSlots[To] = *From;
			}
			Context.Run.Budget += 1;
			Advance(Context, 3);
		}

		/** F0: the scripted camera's yaw, pitch and distance into three variables. */
		void GetScriptCamera(FEventContext& Context)
		{
			const FFieldCamera& Camera = Context.Camera;
			SetVar(Context, ReadU16(Context, 1), Camera.ScriptYaw);
			SetVar(Context, ReadU16(Context, 3), Camera.ScriptPitch);
			SetVar(Context, ReadU16(Context, 5), Camera.ScriptDistance);
			Context.Run.Budget += 1;
			Advance(Context, 7);
		}

		/**
		 * F3: the yaw, elevation (degrees) and half the distance of the path's eye end seen from its
		 * target end, into three variables, as EC orbits (the scripted camera's scale back to 1).
		 */
		void GetPathOrbit(FEventContext& Context)
		{
			FFieldCamera& Camera = Context.Camera;
			const FFieldPoint& Target = Camera.TargetTo;
			const FFieldPoint& Eye = Camera.EyeTo;
			auto Whole = [](int32 From, int32 To) { return Wrapped(static_cast<uint32>(To) - static_cast<uint32>(From)) >> 16; };
			int32 Distance = 0;
			if (!Length(Context, Whole(Target[0], Eye[0]), Whole(Target[1], Eye[1]), Whole(Target[2], Eye[2]), Distance))
			{
				return;
			}
			Camera.ScriptScale = WholeScale;
			const int32 Yaw = GteRatan2(Wrapped(static_cast<uint32>(Target[2]) - static_cast<uint32>(Eye[2])), Wrapped(static_cast<uint32>(Target[0]) - static_cast<uint32>(Eye[0])),
				*Context.Arctangents);
			const int32 Across = FieldLength2D(Whole(Target[0], Eye[0]), Whole(Target[2], Eye[2]));
			const int32 Elevation = GteRatan2(Across, Whole(Eye[1], Target[1]), *Context.Arctangents);
			SetVar(Context, ReadU16(Context, 1), static_cast<int32>((static_cast<uint32>(-Yaw) & 0xFFFF) - 0x400) & 0xFFF);
			SetVar(Context, ReadU16(Context, 3), (Elevation * -0x168 >> 12) + 0x5B);
			SetVar(Context, ReadU16(Context, 5), Distance / 2);
			Context.Run.Budget += 1;
			Advance(Context, 7);
		}
	}

	void RegisterCameraOrbits(FBaseTable& Base, FExtendedTable&)
	{
		Base[0xEB] = &OrbitAroundPoint;
		Base[0xEC] = &OrbitAroundPathSlot;
		Base[0xED] = &GetPathSlot;
		Base[0xEE] = &CopyPathSlot;
		Base[0xF0] = &GetScriptCamera;
		Base[0xF3] = &GetPathOrbit;
	}
}
