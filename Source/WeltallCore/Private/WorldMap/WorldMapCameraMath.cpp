#include "WorldMap/WorldMapOps.h"

#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"

namespace Weltall::WorldMapOps
{
	void LookAtCamera(FWorldMapContext& Context)
	{
		const FWorldMapOrbit& Orbit = Context.Orbit;
		const FGteLongVector Ahead = VectorNormal({Orbit.Target[0] - Orbit.Eye[0], Orbit.Target[1] - Orbit.Eye[1], Orbit.Target[2] - Orbit.Eye[2]});
		const FGteLongVector Side = VectorNormal(OuterProduct12(Ahead, Orbit.Up));
		const FGteLongVector Up = VectorNormal(OuterProduct12(Ahead, Side));
		FGteMatrix& Matrix = Context.CameraMatrix;
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			Matrix.M[0][Axis] = static_cast<int16>(Side[Axis]);
			Matrix.M[1][Axis] = static_cast<int16>(Up[Axis]);
			Matrix.M[2][Axis] = static_cast<int16>(Ahead[Axis]);
		}
		const FGteVector Eye{static_cast<int16>(-Orbit.Eye[0]), static_cast<int16>(-Orbit.Eye[1]), static_cast<int16>(-Orbit.Eye[2])};
		const FGteLongVector Translation = ApplyMatrix(Matrix, Eye);
		Matrix.T = {Translation[0], Translation[1], Translation[2]};
	}

	void AnglesFromMatrix(const FGteMatrix& Matrix, const FGteArctangents& Arctangents, FGteVector& Angles)
	{
		if (Matrix.M[2][0] == 0 && Matrix.M[2][2] == 0)
		{
			return;
		}
		Angles[1] = static_cast<int16>(GteRatan2(Matrix.M[2][0], Matrix.M[2][2], Arctangents) & 0xFFF);
		const FGteMatrix Levelled = MulMatrix(Matrix, RotationY(Angles[1]));
		Angles[0] = static_cast<int16>(GteRatan2(Levelled.M[1][2], Levelled.M[1][1], Arctangents));
		const FGteMatrix Upright = MulMatrix(Levelled, RotationX(Angles[0]));
		Angles[2] = static_cast<int16>(-GteRatan2(Upright.M[1][0], Upright.M[1][1], Arctangents));
	}

	std::array<int32, 3> QuadraticBlend(int32 Fraction, const std::array<int16, 4>& First, const std::array<int16, 4>& Second, const std::array<int16, 4>& Third)
	{
		const int32 Rest = 0x1000 - Fraction;
		const int32 WeightFirst = Rest * Rest * 8 >> 12;
		const int32 WeightSecond = (Rest * Fraction >> 8) + 0x8000;
		const int32 WeightThird = Fraction * Fraction * 8 >> 12;
		std::array<int32, 3> Out{};
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			// The products wrap as the CPU's do.
			Out[Axis] = static_cast<int32>(static_cast<uint32>(First[Axis] * WeightFirst) + static_cast<uint32>(Second[Axis] * WeightSecond)
				+ static_cast<uint32>(Third[Axis] * WeightThird));
		}
		return Out;
	}
}
