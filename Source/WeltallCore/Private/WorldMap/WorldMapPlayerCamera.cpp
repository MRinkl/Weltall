#include "WorldMap/WorldMapPlayerCamera.h"

#include "Field/FieldPlayerCamera.h"
#include "Graphics/Gte.h"
#include "Graphics/GteVectors.h"
#include "WorldMap/WorldMapContext.h"

#include <algorithm>
#include <cmath>

namespace Weltall
{
	namespace
	{
		constexpr double YawUnitsPerDegree = 4096.0 / 360.0;
		constexpr double RadiansPerYawUnit = 6.283185307179586476925 / 4096.0;
		constexpr float HalfTurn = 180.0f;
		/** The steepest the tilt makes the camera look down: 85 degrees. */
		constexpr double SteepestPitch = -85.0 * YawUnitsPerDegree;
		constexpr int32 DistanceOne = 0x1000;

		using FRotation = std::array<std::array<double, 3>, 3>;

		FRotation Product(const FRotation& A, const FRotation& B)
		{
			FRotation Result{};
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				for (std::size_t Column = 0; Column < 3; ++Column)
				{
					Result[Row][Column] = A[Row][0] * B[0][Column] + A[Row][1] * B[1][Column] + A[Row][2] * B[2][Column];
				}
			}
			return Result;
		}

		// The kernel's rotations about each axis, as RotationX, RotationY and RotationZ make them.
		FRotation AboutX(double Angle)
		{
			const double Sin = std::sin(Angle);
			const double Cos = std::cos(Angle);
			return {{{1.0, 0.0, 0.0}, {0.0, Cos, -Sin}, {0.0, Sin, Cos}}};
		}

		FRotation AboutY(double Angle)
		{
			const double Sin = std::sin(Angle);
			const double Cos = std::cos(Angle);
			return {{{Cos, 0.0, Sin}, {0.0, 1.0, 0.0}, {-Sin, 0.0, Cos}}};
		}

		FRotation AboutZ(double Angle)
		{
			const double Sin = std::sin(Angle);
			const double Cos = std::cos(Angle);
			return {{{Cos, -Sin, 0.0}, {Sin, Cos, 0.0}, {0.0, 0.0, 1.0}}};
		}

		/** The game's pitch made steeper by the tilt, in the yaw's units: no steeper than the steepest, unless the game's own is. */
		double TiltedPitch(double Pitch, double Tilt)
		{
			return Tilt > 0.0 ? std::max(Pitch - Tilt, std::min(Pitch, SteepestPitch)) : Pitch - Tilt;
		}

		float Wrapped(float Degrees)
		{
			const float Turn = std::remainder(Degrees, 2.0f * HalfTurn);
			return Turn <= -HalfTurn ? Turn + 2.0f * HalfTurn : Turn;
		}
	}

	std::array<int16, 3> WorldMapOrbitEye(int16 TargetY, int32 Distance, int16 Pitch, int16 Yaw)
	{
		const FGteMatrix Turn = RotationYxz({Pitch, Yaw, 0});
		const std::array<int32, 3> Eye = ApplyMatrixLV(Turn, {0, 0, -(Distance >> 12)});
		return {static_cast<int16>(Eye[0]), static_cast<int16>(TargetY + static_cast<int16>(Eye[1])), static_cast<int16>(Eye[2])};
	}

	FGteMatrix WorldMapCameraMatrix(const std::array<int16, 3>& Eye, int16 Pitch, int16 Yaw, int16 Roll)
	{
		FGteMatrix Matrix;
		Matrix.M = MulMatrix(RotationZ(-Roll), MulMatrix(RotationX(-Pitch), RotationY(-Yaw))).M;
		const FGteVector Back{static_cast<int16>(-Eye[0]), static_cast<int16>(-Eye[1]), static_cast<int16>(-Eye[2])};
		const FGteLongVector Translation = ApplyMatrix(Matrix, Back);
		Matrix.T = {Translation[0], Translation[1], Translation[2]};
		return Matrix;
	}

	std::optional<FWorldMapCameraBasis> WorldMapCameraBasis(const FWorldMapContext& Context)
	{
		if (Context.bCutscene || Context.LookAt != 0)
		{
			return std::nullopt;
		}
		const FWorldMapCameraBasis Basis{Context.Orbit.Target[1], Context.CameraDistance, Context.CameraPitch, Context.CameraYaw, Context.CameraRoll};
		const FWorldMapPlayerView Own = WorldMapPlayerView(Basis, {});
		if (Own.Eye != Context.Orbit.Eye || !(Own.Camera == Context.CameraMatrix))
		{
			return std::nullopt;
		}
		return Basis;
	}

	FWorldMapPlayerView WorldMapPlayerView(const FWorldMapCameraBasis& Basis, const FWorldMapCameraOffsets& Offsets)
	{
		const int32 Turn = static_cast<int32>(std::lround(Offsets.Turn * YawUnitsPerDegree));
		const int32 Whole = Basis.Distance >> 12;
		const int32 Scaled = static_cast<int32>(std::lround(Whole * static_cast<double>(Offsets.Zoom)));
		FWorldMapPlayerView View;
		View.Yaw = static_cast<int16>(Basis.Yaw + Turn);
		View.Pitch = static_cast<int16>(std::lround(TiltedPitch(Basis.Pitch, std::round(Offsets.Tilt * YawUnitsPerDegree))));
		View.Eye = WorldMapOrbitEye(Basis.TargetY, Scaled * DistanceOne, View.Pitch, View.Yaw);
		View.Camera = WorldMapCameraMatrix(View.Eye, View.Pitch, View.Yaw, Basis.Roll);
		View.EyeBack = Scaled - Whole;
		View.Shown.Zoom = Whole != 0 ? static_cast<float>(Scaled) / static_cast<float>(Whole) : 1.0f;
		View.Shown.Tilt = static_cast<float>((Basis.Pitch - View.Pitch) / YawUnitsPerDegree);
		View.Shown.Turn = static_cast<float>(Turn / YawUnitsPerDegree);
		return View;
	}

	FAffine WorldMapPlayerViewAffine(const FWorldMapCameraBasis& Basis, const FWorldMapCameraOffsets& Offsets)
	{
		const double Yaw = (Basis.Yaw + Offsets.Turn * YawUnitsPerDegree) * RadiansPerYawUnit;
		const double Pitch = TiltedPitch(Basis.Pitch, Offsets.Tilt * YawUnitsPerDegree) * RadiansPerYawUnit;
		const double Roll = Basis.Roll * RadiansPerYawUnit;
		const double Distance = (Basis.Distance >> 12) * static_cast<double>(Offsets.Zoom);
		const FRotation Rotation = Product(AboutZ(-Roll), Product(AboutX(-Pitch), AboutY(-Yaw)));
		// The orbit's eye: the distance behind the target, pitched and then turned.
		const std::array<double, 3> Eye{-Distance * std::sin(Yaw) * std::cos(Pitch), Basis.TargetY + Distance * std::sin(Pitch),
			-Distance * std::cos(Yaw) * std::cos(Pitch)};
		FAffine Camera;
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			double Translation = 0.0;
			for (std::size_t Column = 0; Column < 3; ++Column)
			{
				Camera.M[Row][Column] = static_cast<float>(Rotation[Row][Column]);
				Translation -= Rotation[Row][Column] * Eye[Column];
			}
			Camera.T[Row] = static_cast<float>(Translation);
		}
		return Camera;
	}

	FAffine WorldMapPlayerSkyAffine(const FWorldMapCameraBasis& Basis, const FWorldMapCameraOffsets& Offsets)
	{
		const FAffine Camera = WorldMapPlayerViewAffine(Basis, Offsets);
		const FRotation Yaw = AboutY((Basis.Yaw + Offsets.Turn * YawUnitsPerDegree) * RadiansPerYawUnit);
		FAffine Sky = Camera;
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			for (std::size_t Column = 0; Column < 3; ++Column)
			{
				Sky.M[Row][Column] = static_cast<float>(Camera.M[Row][0] * Yaw[0][Column] + Camera.M[Row][1] * Yaw[1][Column] + Camera.M[Row][2] * Yaw[2][Column]);
			}
		}
		return Sky;
	}

	int16 WorldMapViewTurn(const FWorldMapCameraOffsets& Offsets)
	{
		return static_cast<int16>(std::lround(Offsets.Turn * YawUnitsPerDegree));
	}

	FWorldMapCameraOffsets MoveWorldMapCameraOffsets(const FWorldMapCameraOffsets& Offsets, const FFieldViewInput& Input)
	{
		if (Input.bReset)
		{
			return {};
		}
		FWorldMapCameraOffsets Moved;
		Moved.Zoom = std::clamp(Offsets.Zoom * std::exp2(-Input.Zoom), FWorldMapCameraOffsets::MinZoom, FWorldMapCameraOffsets::MaxZoom);
		Moved.Tilt = std::clamp(Offsets.Tilt + Input.Tilt, FWorldMapCameraOffsets::MinTilt, FWorldMapCameraOffsets::MaxTilt);
		Moved.Turn = Wrapped(Offsets.Turn + Input.Turn);
		return Moved;
	}
}
