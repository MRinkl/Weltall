#include "Graphics/Gte.h"

#include "Graphics/GteArithmetic.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>

namespace Weltall
{
	namespace
	{
		using GteArithmetic::Accumulate;
		using GteArithmetic::RowTimes;

		constexpr std::array<uint32, 3> IrSaturated = {GteFlag::Ir1Saturated, GteFlag::Ir2Saturated, GteFlag::Ir3Saturated};
		constexpr uint32 Mac0PositiveOverflow = 1u << 16;
		constexpr uint32 Mac0NegativeOverflow = 1u << 15;
		constexpr uint32 ErrorBits = 0x7F87E000;

		/** The division unit's reciprocal table (psx-spx, "GTE Division Inaccuracy"). */
		constexpr std::array<uint16, 0x101> MakeReciprocals()
		{
			std::array<uint16, 0x101> Table{};
			for (int32 Index = 0; Index < 0x101; ++Index)
			{
				Table[static_cast<std::size_t>(Index)] = static_cast<uint16>(std::max(0, (0x40000 / (Index + 0x100) + 1) / 2 - 0x101));
			}
			return Table;
		}

		constexpr std::array<uint16, 0x101> Reciprocals = MakeReciprocals();

		int64 CheckMac0(int64 Value, uint32& Flag)
		{
			if (Value > 0x7FFFFFFF)
			{
				Flag |= Mac0PositiveOverflow;
			}
			else if (Value < -0x80000000LL)
			{
				Flag |= Mac0NegativeOverflow;
			}
			return Value;
		}

		int64 Saturate(int64 Value, int64 Minimum, int64 Maximum, uint32 FlagBit, uint32& Flag)
		{
			if (Value < Minimum || Value > Maximum)
			{
				Flag |= FlagBit;
			}
			return std::clamp(Value, Minimum, Maximum);
		}

		/** H / SZ3 as the division unit computes it, 16.16, at most 1FFFFh. */
		uint32 Divide(uint16 ProjectionDistance, uint16 ScreenZ, uint32& Flag)
		{
			if (ProjectionDistance >= ScreenZ * 2)
			{
				Flag |= GteFlag::DivideOverflow;
				return 0x1FFFF;
			}
			const int32 Shift = std::countl_zero(ScreenZ);
			const uint32 Numerator = uint32{ProjectionDistance} << Shift;
			uint32 Divisor = uint32{ScreenZ} << Shift;
			const uint32 Reciprocal = Reciprocals[(Divisor - 0x7FC0) >> 7] + 0x101u;
			Divisor = (0x2000080u - Divisor * Reciprocal) >> 8;
			Divisor = (0x0000080u + Divisor * Reciprocal) >> 8;
			return static_cast<uint32>(std::min<uint64>(0x1FFFF, (uint64{Numerator} * Divisor + 0x8000) >> 16));
		}

		/** A MAC value shifted by sf into IR under lm = 1. */
		int32 SaturatePositive(int64 Sum)
		{
			return static_cast<int32>(std::clamp<int64>(static_cast<int32>(Sum >> 12), 0, 0x7FFF));
		}

		uint8 ColorChannel(int64 Sum)
		{
			return static_cast<uint8>(std::clamp<int32>(static_cast<int32>(Sum >> 12) >> 4, 0, 0xFF));
		}

		/**
		 * The normal-color commands' first two steps: IR = LLM * N, then IR = BK + LCM * IR. Returns
		 * the second step's full sums (before the shift), which the color FIFO reads.
		 */
		std::array<int64, 3> LightSums(const FGte& Gte, const FGteVector& Normal)
		{
			uint32 Flag = 0;
			std::array<int32, 3> Light{};
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				Light[Row] = SaturatePositive(RowTimes(Gte.LightMatrix, Row, {Normal[0], Normal[1], Normal[2]}, Flag));
			}
			std::array<int64, 3> Sums{};
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				Sums[Row] = RowTimes(Gte.ColorMatrix, Row, Light, Flag, int64{Gte.BackColor[Row]} * 4096);
			}
			return Sums;
		}

		uint16 OrderingDepth(int64 Sum)
		{
			return static_cast<uint16>(std::clamp<int64>(Sum >> 12, 0, 0xFFFF));
		}

		const std::array<int16, 4096>& SineTable()
		{
			static const std::array<int16, 4096> Table = []
			{
				std::array<int16, 4096> Values{};
				for (std::size_t Angle = 0; Angle < Values.size(); ++Angle)
				{
					Values[Angle] = static_cast<int16>(std::lround(4096.0 * std::sin(static_cast<double>(Angle) * 2.0 * std::numbers::pi / 4096.0)));
				}
				return Values;
			}();
			return Table;
		}
	}

	FGteProjection FGte::Rtps(const FGteVector& Vertex) const
	{
		uint32 Flag = 0;
		std::array<int32, 3> Mac{};
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			int64 Sum = Accumulate(Row, int64{Transform.T[Row]} * 4096, Flag);
			for (std::size_t Column = 0; Column < 3; ++Column)
			{
				Sum = Accumulate(Row, Sum + int64{Transform.M[Row][Column]} * Vertex[Column], Flag);
			}
			Mac[Row] = static_cast<int32>(Sum >> 12);
		}
		std::array<int64, 3> Ir{};
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			Ir[Row] = Saturate(Mac[Row], -0x8000, 0x7FFF, IrSaturated[Row], Flag);
		}

		FGteProjection Result;
		Result.View = Mac;
		Result.ScreenZ = static_cast<uint16>(Saturate(Mac[2], 0, 0xFFFF, GteFlag::DepthSaturated, Flag));
		const int64 Quotient = Divide(ProjectionDistance, Result.ScreenZ, Flag);
		const int64 X = CheckMac0(Quotient * Ir[0] + OffsetX, Flag);
		const int64 Y = CheckMac0(Quotient * Ir[1] + OffsetY, Flag);
		Result.ScreenX = static_cast<int16>(Saturate(X >> 16, -0x400, 0x3FF, GteFlag::ScreenXSaturated, Flag));
		Result.ScreenY = static_cast<int16>(Saturate(Y >> 16, -0x400, 0x3FF, GteFlag::ScreenYSaturated, Flag));
		const int64 Cue = CheckMac0(Quotient * DepthCueA + DepthCueB, Flag);
		Result.DepthCue = static_cast<int16>(Saturate(Cue >> 12, 0, 0x1000, GteFlag::DepthCueSaturated, Flag));
		if ((Flag & ErrorBits) != 0)
		{
			Flag |= GteFlag::Error;
		}
		Result.Flag = Flag;
		return Result;
	}

	FGteColor FGte::NormalColor(const FGteVector& Normal) const
	{
		const std::array<int64, 3> Sums = LightSums(*this, Normal);
		return {ColorChannel(Sums[0]), ColorChannel(Sums[1]), ColorChannel(Sums[2]), 0};
	}

	FGteColor FGte::NormalColorColor(const FGteVector& Normal, const FGteColor& Color) const
	{
		const std::array<int64, 3> Sums = LightSums(*this, Normal);
		const std::array<uint8, 3> Channels = {Color.R, Color.G, Color.B};
		std::array<uint8, 3> Lit{};
		uint32 Flag = 0;
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			Lit[Row] = ColorChannel(Accumulate(Row, int64{Channels[Row]} * SaturatePositive(Sums[Row]) << 4, Flag));
		}
		return {Lit[0], Lit[1], Lit[2], Color.Code};
	}

	FGteColor FGte::DepthCueColor(const FGteColor& Color, int16 DepthCue) const
	{
		uint32 Flag = 0;
		const std::array<uint8, 3> Channels = {Color.R, Color.G, Color.B};
		std::array<uint8, 3> Faded{};
		for (std::size_t Index = 0; Index < 3; ++Index)
		{
			const int64 Start = int64{Channels[Index]} << 16;
			// The step toward the far color saturates as under lm = 0, whatever lm says.
			const int64 Toward = Saturate(Accumulate(Index, int64{FarColor[Index]} * 4096 - Start, Flag) >> 12, -0x8000, 0x7FFF, IrSaturated[Index], Flag);
			Faded[Index] = ColorChannel(Accumulate(Index, Toward * DepthCue + Start, Flag));
		}
		return {Faded[0], Faded[1], Faded[2], Color.Code};
	}

	bool FGte::SetFogNearFar(int32 Near, int32 Far, int32 H)
	{
		const int32 Span = Far - Near;
		if (Span <= 99)
		{
			return true;
		}
		if (H == 0)
		{
			return false;
		}
		const auto Wrap = [](uint32 Value) { return static_cast<int32>(Value); };
		const int32 NearProduct = Wrap((0u - static_cast<uint32>(Near)) * static_cast<uint32>(Far));
		const int32 Scaled = Wrap(static_cast<uint32>(NearProduct / Span) << 8);
		DepthCueA = static_cast<int16>(std::clamp(Scaled / H, -0x8000, 0x7FFF));
		DepthCueB = Wrap(static_cast<uint32>(Wrap(static_cast<uint32>(Far) << 12) / Span) << 12);
		return true;
	}

	uint16 FGte::AverageDepth3(uint16 Depth1, uint16 Depth2, uint16 Depth3) const
	{
		return OrderingDepth(int64{AverageScale3} * (int64{Depth1} + Depth2 + Depth3));
	}

	uint16 FGte::AverageDepth4(uint16 Depth0, uint16 Depth1, uint16 Depth2, uint16 Depth3) const
	{
		return OrderingDepth(int64{AverageScale4} * (int64{Depth0} + Depth1 + Depth2 + Depth3));
	}

	int32 Nclip(int16 X0, int16 Y0, int16 X1, int16 Y1, int16 X2, int16 Y2)
	{
		const int64 Area = int64{X0} * Y1 + int64{X1} * Y2 + int64{X2} * Y0 - int64{X0} * Y2 - int64{X1} * Y0 - int64{X2} * Y1;
		return static_cast<int32>(Area);
	}

	int32 GteSin(int32 Angle)
	{
		return SineTable()[static_cast<std::size_t>(Angle & 0xFFF)];
	}

	int32 GteCos(int32 Angle)
	{
		return GteSin(Angle + 1024);
	}

	FGteMatrix RotationY(int32 Angle)
	{
		const int16 Sin = static_cast<int16>(GteSin(Angle));
		const int16 Cos = static_cast<int16>(GteCos(Angle));
		FGteMatrix Result;
		Result.M[0][0] = Cos;
		Result.M[0][2] = Sin;
		Result.M[1][1] = FGteMatrix::One;
		Result.M[2][0] = static_cast<int16>(-Sin);
		Result.M[2][2] = Cos;
		return Result;
	}

	FGteMatrix RotationX(int32 Angle)
	{
		const int16 Sin = static_cast<int16>(GteSin(Angle));
		const int16 Cos = static_cast<int16>(GteCos(Angle));
		FGteMatrix Result;
		Result.M[0][0] = FGteMatrix::One;
		Result.M[1][1] = Cos;
		Result.M[1][2] = static_cast<int16>(-Sin);
		Result.M[2][1] = Sin;
		Result.M[2][2] = Cos;
		return Result;
	}

	FGteMatrix RotationZ(int32 Angle)
	{
		const int16 Sin = static_cast<int16>(GteSin(Angle));
		const int16 Cos = static_cast<int16>(GteCos(Angle));
		FGteMatrix Result;
		Result.M[0][0] = Cos;
		Result.M[0][1] = static_cast<int16>(-Sin);
		Result.M[1][0] = Sin;
		Result.M[1][1] = Cos;
		Result.M[2][2] = FGteMatrix::One;
		return Result;
	}

	FGteMatrix MulMatrix(const FGteMatrix& A, const FGteMatrix& B)
	{
		FGteMatrix Result;
		Result.T = A.T;
		uint32 Flag = 0;
		for (std::size_t Column = 0; Column < 3; ++Column)
		{
			const std::array<int32, 3> Vector = {B.M[0][Column], B.M[1][Column], B.M[2][Column]};
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				Result.M[Row][Column] = static_cast<int16>(std::clamp<int64>(RowTimes(A, Row, Vector, Flag) >> 12, -0x8000, 0x7FFF));
			}
		}
		return Result;
	}

	std::array<int32, 3> ApplyMatrixLV(const FGteMatrix& Matrix, const std::array<int32, 3>& Vector)
	{
		// Each component is split into its high bits (above 15) and low 15 bits, magnitudes signed alike.
		std::array<int32, 3> High{};
		std::array<int32, 3> Low{};
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			const int32 Value = Vector[Axis];
			const uint32 Magnitude = Value < 0 ? 0u - static_cast<uint32>(Value) : static_cast<uint32>(Value);
			const int32 Sign = Value < 0 ? -1 : 1;
			// The halves pass through the 16-bit IR registers.
			High[Axis] = static_cast<int16>(static_cast<uint16>(Sign * static_cast<int32>(Magnitude >> 15)));
			Low[Axis] = Sign * static_cast<int32>(Magnitude & 0x7FFF);
		}
		uint32 Flag = 0;
		std::array<int32, 3> Result{};
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			const uint32 HighPart = static_cast<uint32>(static_cast<int32>(RowTimes(Matrix, Row, High, Flag)));
			const uint32 LowPart = static_cast<uint32>(static_cast<int32>(RowTimes(Matrix, Row, Low, Flag) >> 12));
			Result[Row] = static_cast<int32>((HighPart << 3) + LowPart);
		}
		return Result;
	}

	FGteMatrix Transpose(const FGteMatrix& Matrix)
	{
		FGteMatrix Result;
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			for (std::size_t Column = 0; Column < 3; ++Column)
			{
				Result.M[Row][Column] = Matrix.M[Column][Row];
			}
		}
		return Result;
	}

	FGteMatrix ScaleRows(const FGteMatrix& Matrix, const std::array<int32, 3>& Scale)
	{
		FGteMatrix Result = Matrix;
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			for (std::size_t Column = 0; Column < 3; ++Column)
			{
				const uint32 Product = static_cast<uint32>(int32{Matrix.M[Row][Column]}) * static_cast<uint32>(Scale[Row]);
				Result.M[Row][Column] = static_cast<int16>(static_cast<uint16>(static_cast<uint32>(static_cast<int32>(Product) >> 12)));
			}
		}
		return Result;
	}
}
