#pragma once

#include "Field/FieldCamera.h"
#include "Foundation/Types.h"

namespace Weltall
{
	class FPsxRandom;
	struct FEventContext;

	/** The buttons of the field's held pad word that turn the camera. */
	namespace FieldCameraButtons
	{
		constexpr uint16 TurnLeft = 0x4;
		constexpr uint16 TurnRight = 0x8;
	}

	/** A direction's bit in the camera's direction masks: direction d (the yaw >> 9) is bit (d + 4) & 7. */
	WELTALLCORE_API uint8 FieldCameraDirectionBit(int32 Direction);

	/**
	 * The directions of Mask met stepping from Direction by Step (1 or -1) before the first one not
	 * in it; 0 when all eight are in it.
	 */
	WELTALLCORE_API int32 CountFieldCameraMaskedSteps(uint8 Mask, int32 Direction, int32 Step);

	/**
	 * One tick of the camera's yaw: a camera standing in a forbidden direction starts turning out of
	 * it toward the side with fewer forbidden steps, one in a direction it leaves starts turning on,
	 * L1 and R1 start a turn toward an allowed direction, and a turn under way moves the yaw by its
	 * step until it lands on the yaw goal. 0xFF in either mask stops every new turn.
	 */
	WELTALLCORE_API void UpdateFieldCameraYaw(FFieldCamera& Camera, uint16 PadHeld);

	/**
	 * One tick of the camera's easing: the projection distance's ramp, the snap, the eye and the
	 * target moving a divisor's share of the way to their goals unless within the divisor of them
	 * (in whole units, per axis), and the shake, which draws three random numbers while it is on.
	 */
	WELTALLCORE_API void SmoothFieldCamera(FFieldCamera& Camera, FPsxRandom& Random);

	/**
	 * The camera's tick, after the actors have moved: the view's directions as the tick found them,
	 * then the scripted moves or the follow of the followed actor through the camera's area with the
	 * player's turns, the easing and the shake, and the view the frame is drawn with. False, with
	 * nothing changed, where the camera would follow an actor without a script state or the map's
	 * camera layer has no triangles.
	 */
	WELTALLCORE_API bool UpdateFieldCamera(FEventContext& Context);
}
