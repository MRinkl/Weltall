#include "EventOps.h"

#include "Graphics/Gte.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"

namespace Weltall::EventOps
{
	namespace
	{
		/** 6D and 6E: radius times the sine or cosine of the angle (4096 a turn), over 4096. */
		template <int32 (*Wave)(int32)>
		void Trigonometric(FEventContext& Context)
		{
			const uint16 Address = ReadU16(Context, 1);
			const uint8 Flags = CodeByte(Context, 7);
			const int32 Angle = ReadOperand(Context, 3, Flags, 0x40);
			const int32 Radius = ReadOperand(Context, 5, Flags, 0x20);
			SetVar(Context, Address, Wave(Angle) * Radius >> 12);
			Advance(Context, 8);
		}

		/** CA: the angle of (x, y), 4096 a turn. */
		void Arctangent(FEventContext& Context)
		{
			const uint16 Address = ReadU16(Context, 1);
			const uint8 Flags = CodeByte(Context, 7);
			const int32 Y = ReadOperand(Context, 3, Flags, 0x40);
			const int32 X = ReadOperand(Context, 5, Flags, 0x20);
			SetVar(Context, Address, static_cast<int16>(GteRatan2(Y, X, *Context.Arctangents)));
			Advance(Context, 8);
		}

		/** FE 72: turns the angle toward the target the short way by at most the step, stopping at the target. */
		void TurnAngleToward(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 9);
			const int32 From = ReadOperand(Context, 3, Flags, 0x40);
			const int32 To = ReadOperand(Context, 5, Flags, 0x20);
			const int32 Step = ReadOperand(Context, 7, Flags, 0x10);
			int32 Angle = 0;
			if (((From - To) & 0xFFF) < 0x800)
			{
				Angle = From - Step;
				if (((Angle - To) & 0xFFF) > 0x7FF)
				{
					Angle = To;
				}
			}
			else
			{
				Angle = From + Step;
				if (((Angle - To) & 0xFFF) < 0x800)
				{
					Angle = To;
				}
			}
			SetVar(Context, ReadU16(Context, 1), Angle & 0xFFF);
			Advance(Context, 10);
		}

		/** FE 73: the distance between two ground points (x, z). */
		void Distance2D(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 11);
			const int32 X1 = ReadOperand(Context, 3, Flags, 0x40);
			const int32 Z1 = ReadOperand(Context, 5, Flags, 0x20);
			const int32 X2 = ReadOperand(Context, 7, Flags, 0x10);
			const int32 Z2 = ReadOperand(Context, 9, Flags, 0x08);
			const uint16 Address = ReadU16(Context, 1);
			int32 Distance = 0;
			if (Length(Context, X2 - X1, Z2 - Z1, 0, Distance))
			{
				SetVar(Context, Address, Distance);
				Advance(Context, 12);
			}
		}

		/**
		 * FE 76: the distance between two points. The second and third operands of each point share a
		 * flag bit (0x20, then 0x08); bits 0x04 and 0x02 are never read.
		 */
		void Distance3D(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 15);
			const int32 X1 = ReadOperand(Context, 3, Flags, 0x40);
			const int32 Y1 = ReadOperand(Context, 5, Flags, 0x20);
			const int32 Z1 = ReadOperand(Context, 7, Flags, 0x20);
			const int32 X2 = ReadOperand(Context, 9, Flags, 0x10);
			const int32 Y2 = ReadOperand(Context, 11, Flags, 0x08);
			const int32 Z2 = ReadOperand(Context, 13, Flags, 0x08);
			const uint16 Address = ReadU16(Context, 1);
			int32 Distance = 0;
			if (Length(Context, X2 - X1, Z2 - Z1, Y2 - Y1, Distance))
			{
				SetVar(Context, Address, Distance);
				Advance(Context, 16);
			}
		}

		/** Value * To / From in 16.16 fixed point, with the processor's wrap-around. */
		int32 Rescale(int32 Value, int32 From, int32 To)
		{
			const int32 Ratio = ProcessorDivide(static_cast<int32>(static_cast<uint32>(To) << 16), From);
			return static_cast<int32>(static_cast<uint32>(Ratio) * static_cast<uint32>(Value)) >> 16;
		}

		/** FE D3: rescales two values from one range to another, each by its own pair of bounds. */
		void RescalePair(FEventContext& Context)
		{
			const int32 Value1 = ReadValue(Context, 1);
			const int32 Value2 = ReadValue(Context, 3);
			const int32 From1 = ReadValue(Context, 5);
			const int32 From2 = ReadValue(Context, 7);
			const int32 To1 = ReadValue(Context, 9);
			const int32 To2 = ReadValue(Context, 11);
			SetVar(Context, ReadU16(Context, 13), Rescale(Value1, From1, To1));
			SetVar(Context, ReadU16(Context, 15), Rescale(Value2, From2, To2));
			Advance(Context, 17);
		}
	}

	bool Length(FEventContext& Context, int32 X, int32 Y, int32 Z, int32& Out)
	{
		const int32 Sum = static_cast<int32>(static_cast<uint32>(GteSquare(X)) + static_cast<uint32>(GteSquare(Y)) + static_cast<uint32>(GteSquare(Z)));
		if (Sum < 0)
		{
			Fail(Context, EEventFault::SquareRootOutsideTable, static_cast<uint32>(Sum));
			return false;
		}
		Out = SquareRoot0(Sum);
		return true;
	}

	void RegisterMath(FBaseTable& Base, FExtendedTable& Extended)
	{
		Base[0x6D] = &Trigonometric<&GteSin>;
		Base[0x6E] = &Trigonometric<&GteCos>;
		Base[0xCA] = &Arctangent;
		Extended[0x72] = &TurnAngleToward;
		Extended[0x73] = &Distance2D;
		Extended[0x76] = &Distance3D;
		Extended[0xD3] = &RescalePair;
	}
}
