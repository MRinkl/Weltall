#include "Field/FieldCamera.h"

#include <cmath>

namespace Weltall
{
	namespace
	{
		constexpr int32 WorldScale = FieldWorldScale;
		constexpr int32 TargetAboveActor = 32;
		/** Up is -y: the camera's up vector, 16.16. */
		constexpr FFieldPoint Up = {0, 0x1000 << 16, 0};

		int32 WholePart(int32 Value)
		{
			return Value >> 16;
		}

		int32 Wrap(uint32 Value)
		{
			return static_cast<int32>(Value);
		}

		int32 Difference(int32 A, int32 B)
		{
			return Wrap(static_cast<uint32>(A) - static_cast<uint32>(B));
		}

		int32 Sum(int32 A, int32 B)
		{
			return Wrap(static_cast<uint32>(A) + static_cast<uint32>(B));
		}

		int32 Product(int32 A, int32 B)
		{
			return Wrap(static_cast<uint32>(A) * static_cast<uint32>(B));
		}

		/** One of the eye's offsets from the target: (trig * H * +-32 >> 16) * scale * 16, in 32-bit registers. */
		int32 EyeOffset(int32 Trig, const FFieldCamera& Camera, bool bNegate)
		{
			const uint32 Scaled = static_cast<uint32>(Product(Trig, Camera.ProjectionDistance)) << 5;
			const int32 Units = Wrap(bNegate ? 0u - Scaled : Scaled) >> 16;
			return Wrap(static_cast<uint32>(Product(Units, Camera.DistanceScale)) << 4);
		}

		FGteMatrix ScaledByWorld(FGteMatrix Matrix)
		{
			return ScaleColumns(Matrix, {WorldScale, WorldScale, WorldScale});
		}

		using FExact = std::array<double, 3>;

		FExact Exact(const FFieldPoint& Point)
		{
			constexpr double PointOne = 65536.0;
			return {Point[0] / PointOne, Point[1] / PointOne, Point[2] / PointOne};
		}

		FExact Normal(const FExact& Vector)
		{
			const double Length = std::sqrt(Vector[0] * Vector[0] + Vector[1] * Vector[1] + Vector[2] * Vector[2]);
			return Length > 0.0 ? FExact{Vector[0] / Length, Vector[1] / Length, Vector[2] / Length} : Vector;
		}

		FExact Outer(const FExact& A, const FExact& B)
		{
			return {A[1] * B[2] - A[2] * B[1], A[2] * B[0] - A[0] * B[2], A[0] * B[1] - A[1] * B[0]};
		}
	}

	FFieldPoint FollowTarget(const FFieldPoint& Position)
	{
		return {Position[0], Difference(Position[1], TargetAboveActor << 16), Position[2]};
	}

	FFieldPoint FollowEye(const FFieldCamera& Camera, const FFieldPoint& Target)
	{
		// The pitch in degrees, as 4096ths of a turn (91 / 8 each), past three quarters of a turn.
		const int32 Angle = ((Camera.Pitch * 91) >> 3) + 0xC00;
		FFieldPoint Eye = Target;
		Eye[1] = Sum(EyeOffset(GteCos(Angle), Camera, true), Target[1]);
		Eye[2] = Sum(EyeOffset(GteSin(Angle), Camera, false), Target[2]);
		// The eye is turned about the target by the camera's rotation, only in x and z.
		const FGteLongVector Toward = {Difference(Target[0], Eye[0]), Difference(Target[1], Eye[1]), Difference(Target[2], Eye[2])};
		const FGteLongVector Turned = ApplyMatrixLV(RotationXyz(Camera.Rotation), Toward);
		Eye[0] = Sum(Turned[0], Target[0]);
		Eye[2] = Sum(Turned[2], Target[2]);
		return Eye;
	}

	FGteMatrix FieldLookAt(const FFieldPoint& Eye, const FFieldPoint& Target)
	{
		const FGteLongVector Forward = VectorNormal(
			{WholePart(Difference(Target[0], Eye[0])), WholePart(Difference(Target[1], Eye[1])), WholePart(Difference(Target[2], Eye[2]))});
		const FGteLongVector Right = VectorNormal(OuterProduct12({WholePart(Up[0]), WholePart(Up[1]), WholePart(Up[2])}, Forward));
		const FGteLongVector Down = VectorNormal(OuterProduct12(Forward, Right));
		FGteMatrix LookAt;
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			LookAt.M[0][Axis] = static_cast<int16>(Right[Axis]);
			LookAt.M[1][Axis] = static_cast<int16>(Down[Axis]);
			LookAt.M[2][Axis] = static_cast<int16>(Forward[Axis]);
		}
		// The eye's whole parts times the world scale, kept to 16 bits a component.
		const FGteVector ScaledEye = {static_cast<int16>(static_cast<int16>(Eye[0] >> 16) * 3), static_cast<int16>(static_cast<int16>(Eye[1] >> 16) * 3),
			static_cast<int16>(static_cast<int16>(Eye[2] >> 16) * 3)};
		const FGteLongVector Moved = ApplyMatrix(LookAt, ScaledEye);
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			LookAt.T[Axis] = Wrap(0u - static_cast<uint32>(Moved[Axis]));
		}
		return LookAt;
	}

	int32 FieldLength2D(int32 X, int32 Z)
	{
		// The GTE squares the components' low 16 bits.
		const int32 LowX = static_cast<int16>(X);
		const int32 LowZ = static_cast<int16>(Z);
		return SquareRoot0(Wrap(static_cast<uint32>(LowX * LowX) + static_cast<uint32>(LowZ * LowZ)));
	}

	FAffine UnroundedFieldView(const FFieldCamera& Camera, const FFieldPoint& Eye, const FFieldPoint& Target)
	{
		const FExact From = Exact(Eye);
		const FExact To = Exact(Target);
		const FExact Forward = Normal({To[0] - From[0], To[1] - From[1], To[2] - From[2]});
		const FExact Right = Normal(Outer({0.0, 1.0, 0.0}, Forward));
		const FExact Down = Normal(Outer(Forward, Right));
		const std::array<FExact, 3> LookAt{Right, Down, Forward};
		const FAffine Screen = FAffine::From(RotationXyz(Camera.ScreenAngles));
		constexpr double Scale = static_cast<double>(WorldScale) / FGteMatrix::One;
		// The screen's rotation after the look-at, the world scaled: each point Scale times its way from the eye, turned.
		FAffine View;
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			double Translation = 0.0;
			for (std::size_t Column = 0; Column < 3; ++Column)
			{
				double Turned = 0.0;
				for (std::size_t Inner = 0; Inner < 3; ++Inner)
				{
					Turned += Screen.M[Row][Inner] * LookAt[Inner][Column];
				}
				View.M[Row][Column] = static_cast<float>(Turned * Scale);
				Translation -= Turned * Scale * From[Column];
			}
			View.T[Row] = static_cast<float>(Translation);
		}
		return View;
	}

	FFieldView BuildFieldView(const FFieldCamera& Camera)
	{
		return BuildFieldView(Camera, FieldLookAt(Camera.Eye, Camera.Target));
	}

	FFieldView BuildFieldView(const FFieldCamera& Camera, const FGteMatrix& LookAt)
	{
		FFieldView View;
		View.ScreenRotation = RotationXyz(Camera.ScreenAngles);
		const FGteMatrix Turned = CompMatrix(View.ScreenRotation, LookAt);
		View.Rotation = Turned;
		View.View = ScaledByWorld(Turned);
		View.Scene = CompMatrix(View.View, RotationXyz(Camera.WorldAngles));
		// The pitch alone: the look-at from a point at the eye's height and horizontal distance,
		// straight toward the origin.
		const int32 Across = FieldLength2D(WholePart(Difference(Camera.Eye[0], Camera.Target[0])), WholePart(Difference(Camera.Eye[2], Camera.Target[2])));
		const FFieldPoint PitchEye = {0, Difference(Camera.Eye[1], Camera.Target[1]), Product(Across, -0x10000)};
		View.Pitch = FieldLookAt(PitchEye, {0, 0, 0});
		View.Billboard = ScaledByWorld(View.Pitch);
		View.Billboard.T = {};
		View.ProjectionDistance = static_cast<uint16>(Camera.ProjectionDistance);
		return View;
	}
}
