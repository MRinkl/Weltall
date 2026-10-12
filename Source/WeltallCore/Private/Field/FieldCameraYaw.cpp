#include "Field/FieldCameraUpdate.h"

namespace Weltall
{
	namespace
	{
		/** Each turn moves 0x200 of a turn over 8 ticks, 0x40 a tick in the 16.16 turning yaw. */
		constexpr int32 TurnTicks = 8;
		constexpr int32 TurnStep = 0x40 << 16;
		constexpr uint8 AllDirections = 0xFF;

		int32 DirectionOf(int32 Yaw)
		{
			return (Yaw & 0xFFF) >> 9;
		}

		void StartTurn(FFieldCamera& Camera, int32 Sign, bool bByButtons)
		{
			Camera.TurnStep = Sign * TurnStep;
			Camera.YawGoal += Sign * FFieldCamera::YawStep;
			Camera.TurnTicks = TurnTicks;
			Camera.bTurnByButtons = bByButtons;
		}

		/** The turns the camera starts by itself out of a direction it leaves or may not stay in. */
		void TurnOutOfMaskedDirection(FFieldCamera& Camera)
		{
			const int32 Direction = DirectionOf(static_cast<uint16>(Camera.Rotation[1]));
			if ((FieldCameraDirectionBit(Direction) & Camera.LeftDirections) != 0)
			{
				// The goal moves on only after a turn of another step: after any 45-degree turn the
				// camera sweeps 8 ticks the way it last turned and lands back on the goal it had.
				if (Camera.TurnStep != -TurnStep && Camera.TurnStep != TurnStep)
				{
					Camera.TurnStep = TurnStep;
					Camera.YawGoal += FFieldCamera::YawStep;
				}
				Camera.TurnTicks = TurnTicks;
				Camera.bTurnByButtons = false;
			}
			if ((FieldCameraDirectionBit(Direction) & Camera.ForbiddenDirections) != 0)
			{
				const int32 Up = CountFieldCameraMaskedSteps(Camera.ForbiddenDirections, Direction, 1);
				const int32 Down = CountFieldCameraMaskedSteps(Camera.ForbiddenDirections, Direction, -1);
				StartTurn(Camera, Down < Up ? -1 : 1, false);
			}
		}

		bool CanTurnBy(const FFieldCamera& Camera, uint16 PadHeld, uint16 Button, int32 Turn)
		{
			return (PadHeld & Button) != 0 && (Camera.Flags & FieldCameraFlags::NoButtonTurn) == 0 && Camera.TurnTicks == 0
				&& (FieldCameraDirectionBit(DirectionOf(Camera.Rotation[1] + Turn)) & Camera.ForbiddenDirections) == 0;
		}
	}

	uint8 FieldCameraDirectionBit(int32 Direction)
	{
		return static_cast<uint8>(1u << ((Direction + 4) & 7));
	}

	int32 CountFieldCameraMaskedSteps(uint8 Mask, int32 Direction, int32 Step)
	{
		for (int32 Count = 0; Count < 8; ++Count)
		{
			if ((Mask & FieldCameraDirectionBit(Direction + Step * Count)) == 0)
			{
				return Count;
			}
		}
		return 0;
	}

	void UpdateFieldCameraYaw(FFieldCamera& Camera, uint16 PadHeld)
	{
		if (Camera.LeftDirections != AllDirections && Camera.ForbiddenDirections != AllDirections)
		{
			if (Camera.TurnTicks == 0)
			{
				TurnOutOfMaskedDirection(Camera);
			}
			if (CanTurnBy(Camera, PadHeld, FieldCameraButtons::TurnLeft, -FFieldCamera::YawStep))
			{
				StartTurn(Camera, -1, true);
			}
			if (CanTurnBy(Camera, PadHeld, FieldCameraButtons::TurnRight, FFieldCamera::YawStep))
			{
				StartTurn(Camera, 1, true);
			}
		}
		if (Camera.TurnTicks != 0)
		{
			Camera.TurningYaw = static_cast<int32>(static_cast<uint32>(Camera.TurningYaw) + static_cast<uint32>(Camera.TurnStep));
			Camera.Rotation[1] = static_cast<int16>(Camera.TurningYaw >> 16);
			Camera.TurnTicks = static_cast<int16>(Camera.TurnTicks - 1);
			if (Camera.TurnTicks != 0)
			{
				return;
			}
		}
		Camera.Rotation[1] = static_cast<int16>(Camera.YawGoal);
	}
}
