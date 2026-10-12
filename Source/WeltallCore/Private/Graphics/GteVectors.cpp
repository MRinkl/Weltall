#include "Graphics/GteVectors.h"

#include "Graphics/GteArithmetic.h"

#include <algorithm>
#include <bit>
#include <limits>

namespace Weltall
{
	namespace
	{
		using GteArithmetic::Accumulate;
		using GteArithmetic::Mac;
		using GteArithmetic::RowTimes;

		constexpr int32 IntegerSquareRoot(int64 Value)
		{
			int64 Root = 0;
			for (int64 Bit = int64{1} << 30; Bit != 0; Bit >>= 1)
			{
				if ((Root + Bit) * (Root + Bit) <= Value)
				{
					Root += Bit;
				}
			}
			return static_cast<int32>(Root);
		}

		/**
		 * libgte's two tables, indexed by a value normalized to 64..255: floor(512 * sqrt(v)) and
		 * floor(32768 / sqrt(v)), which is 4096 at 64.
		 */
		struct FRootTables
		{
			std::array<int16, 192> Roots{};
			std::array<int16, 192> ReciprocalRoots{};

			constexpr FRootTables()
			{
				for (int32 Index = 0; Index < 192; ++Index)
				{
					const int64 Value = Index + 64;
					Roots[static_cast<std::size_t>(Index)] = static_cast<int16>(IntegerSquareRoot(Value * 512 * 512));
					ReciprocalRoots[static_cast<std::size_t>(Index)] = static_cast<int16>(IntegerSquareRoot((int64{1} << 30) / Value));
				}
			}
		};

		constexpr FRootTables Tables;

		/** The GTE's LZCR for a non-negative value: its leading zero bits. */
		int32 LeadingZeros(uint32 Value)
		{
			return std::countl_zero(Value);
		}

		/** The library's normalization: Value shifted by an even count so its top bits land at 64..255. */
		int32 NormalizedIndex(uint32 Value, int32 EvenLeading)
		{
			const uint32 Normalized = EvenLeading >= 24 ? Value << (EvenLeading - 24) : Value >> (24 - EvenLeading);
			return static_cast<int32>(Normalized) - 64;
		}

		/** The R3000's DIV: truncating, and without a trap for a zero divisor or the one overflow. */
		int32 R3000Divide(int32 Dividend, int32 Divisor)
		{
			if (Divisor == 0)
			{
				return Dividend >= 0 ? -1 : 1;
			}
			if (Dividend == std::numeric_limits<int32>::min() && Divisor == -1)
			{
				return Dividend;
			}
			return Dividend / Divisor;
		}

		std::array<int32, 3> LowHalves(const FGteLongVector& Vector)
		{
			return {static_cast<int16>(Vector[0]), static_cast<int16>(Vector[1]), static_cast<int16>(Vector[2])};
		}

		FGteLongVector OuterProduct(const FGteLongVector& A, const FGteLongVector& B, int32 Shift)
		{
			// The GTE's OP: the rotation diagonal holds A, IR holds B.
			const std::array<int32, 3> D = LowHalves(A);
			const std::array<int32, 3> Ir = LowHalves(B);
			const std::array<std::array<int64, 2>, 3> Terms = {{
				{int64{Ir[2]} * D[1], int64{Ir[1]} * D[2]},
				{int64{Ir[0]} * D[2], int64{Ir[2]} * D[0]},
				{int64{Ir[1]} * D[0], int64{Ir[0]} * D[1]},
			}};
			FGteLongVector Result{};
			uint32 Flag = 0;
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				const int64 Sum = Accumulate(Row, Accumulate(Row, Terms[Row][0], Flag) - Terms[Row][1], Flag);
				Result[Row] = static_cast<int32>(Sum >> Shift);
			}
			return Result;
		}

	}

	int32 LargestComponent(const FGteLongVector& Vector)
	{
		const auto Magnitude = [](int32 Value) { return Value < 0 ? static_cast<int32>(0u - static_cast<uint32>(Value)) : Value; };
		const int32 X = Magnitude(Vector[0]);
		const int32 Y = Magnitude(Vector[1]);
		const int32 Z = Magnitude(Vector[2]);
		if (X >= Y)
		{
			return X >= Z ? Vector[0] : Vector[2];
		}
		return Y >= Z ? Vector[1] : Vector[2];
	}

	FGteLongVector VectorNormal(const FGteLongVector& Vector)
	{
		// SQR, then the three squares added as the library adds them.
		const std::array<int32, 3> Ir = LowHalves(Vector);
		const uint32 Sum = static_cast<uint32>(Ir[0] * Ir[0]) + static_cast<uint32>(Ir[1] * Ir[1]) + static_cast<uint32>(Ir[2] * Ir[2]);
		if (Sum == 0)
		{
			return {0, 0, 0};
		}
		const int32 Leading = LeadingZeros(Sum) & ~1;
		const int32 Shift = (31 - Leading) >> 1;
		const int32 Scale = Tables.ReciprocalRoots[static_cast<std::size_t>(NormalizedIndex(Sum, Leading))];
		// GPF without sf: MAC = IR * IR0, then shifted down by half the normalization.
		return {Ir[0] * Scale >> Shift, Ir[1] * Scale >> Shift, Ir[2] * Scale >> Shift};
	}

	FGteVector VectorNormalS(const FGteLongVector& Vector)
	{
		const FGteLongVector Normal = VectorNormal(Vector);
		return {static_cast<int16>(Normal[0]), static_cast<int16>(Normal[1]), static_cast<int16>(Normal[2])};
	}

	int32 SquareRoot0(int32 Value)
	{
		if (Value <= 0)
		{
			return 0;
		}
		const uint32 Unsigned = static_cast<uint32>(Value);
		const int32 Leading = LeadingZeros(Unsigned) & ~1;
		const int32 Shift = (31 - Leading) >> 1;
		const uint32 Root = static_cast<uint32>(Tables.Roots[static_cast<std::size_t>(NormalizedIndex(Unsigned, Leading))]);
		return static_cast<int32>((Root << Shift) >> 12);
	}

	FGteLongVector OuterProduct12(const FGteLongVector& A, const FGteLongVector& B)
	{
		return OuterProduct(A, B, 12);
	}

	FGteLongVector OuterProduct0(const FGteLongVector& A, const FGteLongVector& B)
	{
		return OuterProduct(A, B, 0);
	}

	FGteLongVector ApplyMatrix(const FGteMatrix& Matrix, const FGteVector& Vector)
	{
		uint32 Flag = 0;
		const std::array<int32, 3> Input = {Vector[0], Vector[1], Vector[2]};
		return {Mac(RowTimes(Matrix, 0, Input, Flag)), Mac(RowTimes(Matrix, 1, Input, Flag)), Mac(RowTimes(Matrix, 2, Input, Flag))};
	}

	FGteLongVector RotTrans(const FGteMatrix& Matrix, const FGteVector& Vector, uint32* Flag)
	{
		uint32 Raised = 0;
		const std::array<int32, 3> Input = {Vector[0], Vector[1], Vector[2]};
		FGteLongVector Result{};
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			Result[Row] = Mac(RowTimes(Matrix, Row, Input, Raised, int64{Matrix.T[Row]} * 4096));
			Raised |= GteArithmetic::IrSaturation(Row, Result[Row]);
		}
		if (Flag != nullptr)
		{
			*Flag = GteArithmetic::WithErrorBit(Raised);
		}
		return Result;
	}

	namespace
	{
		/** An IR register's 16 bits: the MAC value saturated as the GTE's lm = 0 keeps it. */
		FGteVector SaturatedShorts(const FGteLongVector& Mac)
		{
			FGteVector Result{};
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Result[Axis] = static_cast<int16>(std::clamp<int32>(Mac[Axis], -0x8000, 0x7FFF));
			}
			return Result;
		}
	}

	FGteVector ApplyMatrixSV(const FGteMatrix& Matrix, const FGteVector& Vector)
	{
		return SaturatedShorts(ApplyMatrix(Matrix, Vector));
	}

	FGteVector RotTransSV(const FGteMatrix& Matrix, const FGteVector& Vector)
	{
		return SaturatedShorts(RotTrans(Matrix, Vector));
	}

	FGteMatrix CompMatrix(const FGteMatrix& A, const FGteMatrix& B)
	{
		FGteMatrix Result = MulMatrix(A, B);
		uint32 Flag = 0;
		const std::array<int32, 3> Translation = LowHalves(B.T);
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			const uint32 Rotated = static_cast<uint32>(Mac(RowTimes(A, Row, Translation, Flag)));
			Result.T[Row] = static_cast<int32>(Rotated + static_cast<uint32>(A.T[Row]));
		}
		return Result;
	}

	FGteMatrix RotationXyz(const FGteVector& Angles)
	{
		const int32 SinX = GteSin(Angles[0]);
		const int32 CosX = GteCos(Angles[0]);
		const int32 SinY = GteSin(Angles[1]);
		const int32 CosY = GteCos(Angles[1]);
		const int32 SinZ = GteSin(Angles[2]);
		const int32 CosZ = GteCos(Angles[2]);
		const int32 A = (CosZ * -SinY) >> 12;
		const int32 B = (SinZ * -SinY) >> 12;
		const auto Entry = [](int32 Value) { return static_cast<int16>(Value); };
		FGteMatrix Result;
		Result.M = {{
			{Entry((CosZ * CosY) >> 12), Entry(-(SinZ * CosY) >> 12), Entry(SinY)},
			{Entry(((SinZ * CosX) >> 12) - ((A * SinX) >> 12)), Entry(((CosZ * CosX) >> 12) + ((B * SinX) >> 12)), Entry(-(CosY * SinX) >> 12)},
			{Entry(((A * CosX) >> 12) + ((SinZ * SinX) >> 12)), Entry(((CosZ * SinX) >> 12) - ((B * CosX) >> 12)), Entry((CosY * CosX) >> 12)},
		}};
		return Result;
	}

	FGteMatrix RotationZyx(const FGteVector& Angles)
	{
		std::array<int32, 3> Sin{};
		std::array<int32, 3> Cos{};
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			const int32 Angle = Angles[Axis];
			const int32 Magnitude = (Angle < 0 ? -Angle : Angle) & 0xFFF;
			Sin[Axis] = Angle < 0 ? -GteSin(Magnitude) : GteSin(Magnitude);
			Cos[Axis] = GteCos(Magnitude);
		}
		// The library's products keep 32 bits before the shift.
		const auto Product = [](int32 A, int32 B) { return static_cast<int32>(static_cast<uint32>(A) * static_cast<uint32>(B)) >> 12; };
		const auto Entry = [](int32 Value) { return static_cast<int16>(Value); };
		const int32 SinXSinY = Product(Sin[0], Sin[1]);
		const int32 CosXSinY = Product(Sin[1], Cos[0]);
		FGteMatrix Result;
		Result.M = {{
			{Entry(Product(Cos[1], Cos[2])), Entry(Product(SinXSinY, Cos[2]) - Product(Sin[2], Cos[0])), Entry(Product(CosXSinY, Cos[2]) + Product(Sin[0], Sin[2]))},
			{Entry(Product(Sin[2], Cos[1])), Entry(Product(SinXSinY, Sin[2]) + Product(Cos[0], Cos[2])), Entry(Product(CosXSinY, Sin[2]) - Product(Sin[0], Cos[2]))},
			{Entry(-Sin[1]), Entry(Product(Sin[0], Cos[1])), Entry(Product(Cos[0], Cos[1]))},
		}};
		return Result;
	}

	FGteMatrix RotationYxz(const FGteVector& Angles)
	{
		std::array<int32, 3> Sin{};
		std::array<int32, 3> Cos{};
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			const int32 Angle = Angles[Axis];
			const int32 Magnitude = (Angle < 0 ? -Angle : Angle) & 0xFFF;
			Sin[Axis] = Angle < 0 ? -GteSin(Magnitude) : GteSin(Magnitude);
			Cos[Axis] = GteCos(Magnitude);
		}
		const auto Product = [](int32 A, int32 B) { return (A * B) >> 12; };
		const auto Entry = [](int32 Value) { return static_cast<int16>(Value); };
		const int32 SinYSinX = Product(Sin[1], Sin[0]);
		const int32 CosYSinX = Product(Cos[1], Sin[0]);
		FGteMatrix Result;
		Result.M = {{
			{Entry(Product(Cos[1], Cos[2]) + Product(SinYSinX, Sin[2])), Entry(Product(SinYSinX, Cos[2]) - Product(Cos[1], Sin[2])), Entry(Product(Sin[1], Cos[0]))},
			{Entry(Product(Sin[2], Cos[0])), Entry(Product(Cos[2], Cos[0])), Entry(-Sin[0])},
			{Entry(Product(CosYSinX, Sin[2]) - Product(Sin[1], Cos[2])), Entry(Product(Sin[1], Sin[2]) + Product(CosYSinX, Cos[2])), Entry(Product(Cos[1], Cos[0]))},
		}};
		return Result;
	}

	FGteVector TriangleNormal(const FGteVector& P0, const FGteVector& P1, const FGteVector& P2)
	{
		const FGteLongVector ToFirst = {P1[0] - P0[0], P1[1] - P0[1], P1[2] - P0[2]};
		const FGteLongVector ToSecond = {P2[0] - P0[0], P2[1] - P0[1], P2[2] - P0[2]};
		FGteLongVector Normal = OuterProduct0(ToSecond, ToFirst);
		int32 Largest = LargestComponent(Normal);
		if (Largest < 0)
		{
			Largest = static_cast<int32>(0u - static_cast<uint32>(Largest));
		}
		const int32 Root = SquareRoot0(Largest);
		for (int32& Component : Normal)
		{
			Component = R3000Divide(Component, Root);
		}
		return VectorNormalS(Normal);
	}

	bool WindsClockwiseFromEye(const FGteProjection& A, const FGteProjection& B, const FGteProjection& C)
	{
		const auto View = [](const FGteProjection& Corner, std::size_t Axis) { return static_cast<double>(Corner.View[Axis]); };
		const double CrossX = View(B, 1) * View(C, 2) - View(B, 2) * View(C, 1);
		const double CrossY = View(B, 2) * View(C, 0) - View(B, 0) * View(C, 2);
		const double CrossZ = View(B, 0) * View(C, 1) - View(B, 1) * View(C, 0);
		return View(A, 0) * CrossX + View(A, 1) * CrossY + View(A, 2) * CrossZ > 0.0;
	}
}
