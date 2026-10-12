#include "Battle/Show/BattleShowCamera.h"

#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** The screen's centre the framing measures from. */
		constexpr int32 CentreX = 0xA0;
		constexpr int32 CentreY = 0xA4;
		/** Within this (in quarter pixels) of the centre everything fits at the nearest distance. */
		constexpr int32 FitRadius = 0x78;

		/** Adds in 32 bits as the processor does, wrapping. */
		int32 Wrap(int64 Value)
		{
			return static_cast<int32>(static_cast<uint32>(Value));
		}

		/** A 16.16 coordinate's whole part, as the code reads the upper half of the word. */
		int16 Whole(int32 Value)
		{
			return static_cast<int16>(static_cast<uint32>(Value) >> 16);
		}

		bool Framed(uint32 Mask, int32 Slot, const FBattleCameraSubject& Subject)
		{
			return ((Mask & 0xFFFF) >> Slot & 1) != 0 && !Subject.bHidden && Subject.bPresent;
		}

		/** An off-centre distance in quarter pixels, kept to 16 bits as the code keeps it. */
		int32 Offset(int16 Screen, int32 Centre)
		{
			return static_cast<int16>((Screen - Centre) * 4);
		}

		/** One axis of the easing: a 4.12 share of the 16-bit difference through the GTE, saturated. */
		int32 EaseStep(int16 Current, int16 Desired, int32 Rate)
		{
			const int32 Difference = static_cast<int16>(Desired - Current);
			return std::clamp((Difference * Rate) >> 12, -0x8000, 0x7FFF);
		}

		void Ease(FGteVector& Current, const FGteVector& Desired, int32 Rate)
		{
			const std::array<int32, 3> Steps{EaseStep(Current[0], Desired[0], Rate), EaseStep(Current[1], Desired[1], Rate),
				EaseStep(Current[2], Desired[2], Rate)};
			if (Steps[0] == 0 && Steps[1] == 0 && Steps[2] == 0)
			{
				Current = Desired;
				return;
			}
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Current[Axis] = static_cast<int16>(Current[Axis] + Steps[Axis]);
			}
		}
	}

	FGteMatrix BattleLookAt(const FGteVector& Eye, const FGteVector& Target, const FGteVector& Up)
	{
		const FGteLongVector Forward = VectorNormal({Target[0] - Eye[0], Target[1] - Eye[1], Target[2] - Eye[2]});
		const FGteLongVector Across = VectorNormal(OuterProduct12({Up[0], Up[1], Up[2]}, Forward));
		const FGteLongVector Down = VectorNormal(OuterProduct12(Forward, Across));
		FGteMatrix View;
		for (std::size_t Column = 0; Column < 3; ++Column)
		{
			View.M[0][Column] = static_cast<int16>(Across[Column]);
			View.M[1][Column] = static_cast<int16>(Down[Column]);
			View.M[2][Column] = static_cast<int16>(Forward[Column]);
		}
		const FGteLongVector Moved = ApplyMatrix(View, Eye);
		View.T = {-Moved[0], -Moved[1], -Moved[2]};
		return View;
	}

	void FBattleCamera::Init()
	{
		EaseRate = DefaultEaseRate;
		FramedSlots = 0xFFFFFFFF;
		Easing = 1;
		SetMode(BattleCameraModes::Still);
	}

	void FBattleCamera::SetMode(int32 NewMode)
	{
		Easing = 1;
		Mode = NewMode;
		if (NewMode == BattleCameraModes::Controllers)
		{
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				EyePoint[Axis] = DesiredEye[Axis] * 0x10000;
				TargetPoint[Axis] = DesiredTarget[Axis] * 0x10000;
			}
		}
		else if (NewMode == BattleCameraModes::Model)
		{
			Easing = 5;
		}
		else if (Controllers != nullptr)
		{
			Controllers->EndControllers();
		}
	}

	void FBattleCamera::Frame(uint32 Slots, const std::array<FBattleCameraSubject, 11>& Subjects, const FGte& Gte)
	{
		if (!bLocked)
		{
			SetMode(BattleCameraModes::Framing);
			FrameSlots(Slots, Subjects, Gte);
		}
		SpriteDistance = static_cast<uint16>(FramingDistance);
	}

	void FBattleCamera::FrameSlots(uint32 Slots, const std::array<FBattleCameraSubject, 11>& Subjects, const FGte& Gte)
	{
		std::array<int32, 3> Sum{};
		int32 Count = 0;
		for (int32 Slot = 0; Slot < 11; ++Slot)
		{
			const FBattleCameraSubject& Subject = Subjects[static_cast<std::size_t>(Slot)];
			if (Framed(Slots, Slot, Subject))
			{
				++Count;
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Sum[Axis] = Wrap(int64{Sum[Axis]} + (Subject.Position[Axis] >> 1));
				}
			}
		}
		FramedSlots = Slots;
		if (Count == 0)
		{
			return;
		}
		// The box of the framed sprites, grown from their mean (each coordinate halved for the sum).
		std::array<int32, 3> Low{};
		std::array<int32, 3> High{};
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			Low[Axis] = High[Axis] = Wrap(int64{Sum[Axis] / Count} * 2);
		}
		for (int32 Slot = 0; Slot < 11; ++Slot)
		{
			const FBattleCameraSubject& Subject = Subjects[static_cast<std::size_t>(Slot)];
			if (Framed(Slots, Slot, Subject))
			{
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					High[Axis] = std::max(High[Axis], Subject.Position[Axis]);
					Low[Axis] = std::min(Low[Axis], Subject.Position[Axis]);
				}
			}
		}
		FGteVector Centre{};
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			const int32 Both = Wrap(int64{Low[Axis]} + High[Axis]);
			Centre[Axis] = static_cast<int16>((Both - (Both >> 31)) >> 17);
		}

		const FGteMatrix Tilt = RotationZyx({Pitch, 0, 0});
		const FGteLongVector Trial = ApplyMatrix(Tilt, {0, 0, static_cast<int16>(Gte.ProjectionDistance << 3)});
		const FGteVector TrialEye{static_cast<int16>(Centre[0] - Trial[0]), static_cast<int16>(Centre[1] + Trial[1]), static_cast<int16>(Centre[2] - Trial[2])};
		FGte Projection = Gte;
		Projection.Transform = BattleLookAt(TrialEye, Centre, Up);
		int32 Farthest = 0;
		const auto Measure = [&](const FGteVector& Point)
		{
			const FGteProjection Screen = Projection.Rtps(Point);
			const int32 X = Offset(Screen.ScreenX, CentreX);
			const int32 Y = Offset(Screen.ScreenY, CentreY);
			Farthest = std::max(Farthest, X * X + Y * Y);
		};
		for (int32 Slot = 0; Slot < 11; ++Slot)
		{
			const FBattleCameraSubject& Subject = Subjects[static_cast<std::size_t>(Slot)];
			if (!Framed(Slots, Slot, Subject))
			{
				continue;
			}
			FGteVector Point{Whole(Subject.Position[0]), Whole(Subject.Position[1]), Whole(Subject.Position[2])};
			Measure(Point);
			if (Subject.bInGear && !bIgnoresHeights)
			{
				Point[1] = static_cast<int16>(Point[1] - Subject.Height);
				Measure(Point);
			}
		}

		const int32 Reach = SquareRoot0(Farthest);
		int32 Away = 0;
		if (Reach < FitRadius)
		{
			Away = Gte.ProjectionDistance << 1;
		}
		else
		{
			Away = ((Reach * 0x4000) / FitRadius) * 2 * Gte.ProjectionDistance >> 14;
		}
		FramingDistance = Away;
		const FGteLongVector Back = ApplyMatrix(Tilt, {0, 0, static_cast<int16>(Away)});
		DesiredEye = {static_cast<int16>(Centre[0] - Back[0]), static_cast<int16>(Centre[1] + Back[1]), static_cast<int16>(Centre[2] - Back[2])};
		DesiredTarget = Centre;
	}

	void FBattleCamera::TakeForModel()
	{
		if (!bLocked)
		{
			SetMode(BattleCameraModes::Model);
		}
	}

	void FBattleCamera::Release()
	{
		if (!bLocked)
		{
			SetMode(BattleCameraModes::Framing);
		}
	}

	void FBattleCamera::Step(const std::array<FBattleCameraSubject, 11>& Subjects, const FGte& Gte, const FGteArctangents& Arctangents)
	{
		if (Mode == BattleCameraModes::Framing)
		{
			FrameSlots(FramedSlots, Subjects, Gte);
		}
		else if (Mode == BattleCameraModes::Controllers)
		{
			DesiredEye = {Whole(EyePoint[0]), Whole(EyePoint[1]), Whole(EyePoint[2])};
			DesiredTarget = {Whole(TargetPoint[0]), Whole(TargetPoint[1]), Whole(TargetPoint[2])};
		}
		if (Easing == 1)
		{
			Ease(Eye, DesiredEye, EaseRate);
			Ease(Target, DesiredTarget, EaseRate);
		}
		const std::array<int32, 3> Across{Target[0] - Eye[0], Target[1] - Eye[1], Target[2] - Eye[2]};
		const int32 Flat = SquareRoot0(GteSquare(Across[0]) + GteSquare(Across[2]));
		Distance = SquareRoot0(GteSquare(Across[0]) + GteSquare(Across[1]) + GteSquare(Across[2]));
		ViewYaw = static_cast<int16>(-GteRatan2(Across[2], Across[0], Arctangents));
		ViewPitch = static_cast<int16>(-GteRatan2(Across[1], Flat, Arctangents));
	}

	void FBattleCamera::BuildView(const FGteVector& ShakeOffset)
	{
		View = BattleLookAt(Eye, Target, Up);
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			View.T[Axis] += ShakeOffset[Axis];
		}
	}
}
