#pragma once

#include "Battling/BattlingSkeleton.h"
#include "Foundation/ProcessorDivide.h"
#include "Game/PsxRandom.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"

#include <algorithm>
#include <array>

namespace Weltall::BattlingMath
{
	inline int16 Saturate16(int64 Value)
	{
		return static_cast<int16>(std::clamp<int64>(Value, -0x8000, 0x7FFF));
	}

	/**
	 * Steps Angle by Step toward Target the short way round (12 bits a turn), stopping on it; exactly opposite, a coin from
	 * rand() picks the way (odd: down). The result keeps 16 bits, unmasked.
	 */
	inline int32 TurnAngle(int32 Angle, int32 Target, int32 Step, FPsxRandom& Random)
	{
		const uint32 Difference = static_cast<uint32>(Angle - Target) & 0xFFF;
		if (Difference == 0)
		{
			return static_cast<int16>(Angle);
		}
		bool bDown = Difference < 0x800;
		if (Difference == 0x800)
		{
			bDown = (Random.Next() & 1) != 0;
		}
		const int16 Stepped = static_cast<int16>(bDown ? Angle - Step : Angle + Step);
		const uint32 Past = static_cast<uint32>(Stepped - static_cast<int16>(Target)) & 0xFFF;
		const bool bShort = bDown ? Past < 0x801 : Past > 0x7FF;
		return bShort ? Stepped : static_cast<int16>(Target);
	}

	/** The program's lengths: the GTE squares the low 16 bits of each component, and libgte's square root rounds down. */
	inline int32 Length3(int32 X, int32 Y, int32 Z)
	{
		return SquareRoot0(static_cast<int32>(static_cast<uint32>(GteSquare(X)) + static_cast<uint32>(GteSquare(Y)) + static_cast<uint32>(GteSquare(Z))));
	}

	inline int32 Length2(int32 X, int32 Z)
	{
		return SquareRoot0(static_cast<int32>(static_cast<uint32>(GteSquare(X)) + static_cast<uint32>(GteSquare(Z))));
	}

	inline int32 Distance(const FBattlingPoint& A, const FBattlingPoint& B)
	{
		return Length3(B[0] - A[0], B[1] - A[1], B[2] - A[2]);
	}

	inline int32 DistanceXZ(const FBattlingPoint& A, const FBattlingPoint& B)
	{
		return Length2(B[0] - A[0], B[2] - A[2]);
	}

	/** The program's normalization of a long vector: divided by the root of its largest component, then VectorNormalS. */
	inline std::array<int16, 3> NormalizeToShort(const FBattlingPoint& Vector)
	{
		int32 Largest = LargestComponent({Vector[0], Vector[1], Vector[2]});
		if (Largest < 0)
		{
			Largest = static_cast<int32>(0u - static_cast<uint32>(Largest));
		}
		const int32 Root = SquareRoot0(Largest);
		return VectorNormalS({ProcessorDivide(Vector[0], Root), ProcessorDivide(Vector[1], Root), ProcessorDivide(Vector[2], Root)});
	}

	/** The GTE's GPF12: each component times Scale, shifted down 12 and saturated to 16 bits. */
	inline std::array<int16, 3> ScaleVector(const std::array<int16, 3>& Vector, int32 Scale)
	{
		return {Saturate16((static_cast<int64>(Vector[0]) * Scale) >> 12), Saturate16((static_cast<int64>(Vector[1]) * Scale) >> 12),
			Saturate16((static_cast<int64>(Vector[2]) * Scale) >> 12)};
	}

	/** libgte's LoadAverageShort12: A times WeightA plus B times WeightB, each product shifted down 12, saturated. */
	inline std::array<int16, 3> AverageShort12(const std::array<int16, 3>& A, const std::array<int16, 3>& B, int32 WeightA, int32 WeightB)
	{
		std::array<int16, 3> Result{};
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			Result[Axis] = Saturate16(((static_cast<int64>(A[Axis]) * WeightA) >> 12) + ((static_cast<int64>(B[Axis]) * WeightB) >> 12));
		}
		return Result;
	}

	/** The program's division of a product by 256, rounding toward zero. */
	inline int32 Over256(int32 Value)
	{
		return (Value < 0 ? Value + 0xFF : Value) >> 8;
	}

	inline int32 Over4096(int32 Value)
	{
		return (Value < 0 ? Value + 0xFFF : Value) >> 12;
	}

	/** The program's rounded division of Value - Base: away from zero, so an easing step never stalls short of its goal. */
	inline int32 DivideRoundingAway(int32 Value, int32 Base, int32 Divisor)
	{
		const int32 Difference = Value - Base;
		return (Difference < 0 ? Difference + 1 - Divisor : Difference - 1 + Divisor) / Divisor;
	}

	/** Whether HP is above n/255 of the maximum. */
	inline bool HpAbove(int32 Hp, int32 MaxHp, int32 Fraction)
	{
		return MaxHp * Fraction / 0xFF < Hp;
	}
}
