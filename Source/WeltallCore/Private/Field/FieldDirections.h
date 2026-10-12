#pragma once

#include "Field/FieldCamera.h"
#include "Foundation/Types.h"

// Directions in the eighths of a turn the scripts count them in, 0 to 7, as variables 6 and 8 hold
// the leader's and the camera's.
namespace Weltall::FieldDirections
{
	/** An actor's facing (4096 a turn; bit 15 set, it counts as negative) in eighths. */
	inline int32 FacingEighth(uint16 Facing)
	{
		return (((static_cast<int16>(Facing) + 0x100) >> 9) + 2) & 7;
	}

	/** The camera's direction in eighths, counted the other way from the yaw it turns to. */
	inline int32 CameraEighth(const FFieldCamera& Camera)
	{
		return (7 - ((Camera.GoalViewYaw - 0x100) >> 9)) & 7;
	}

	/** The direction of the view the player is shown in the same eighths: the camera's, turned by ViewTurn (4096 a turn). */
	inline int32 ShownCameraEighth(const FFieldCamera& Camera, int16 ViewTurn)
	{
		return (7 - ((Camera.GoalViewYaw + ViewTurn - 0x100) >> 9)) & 7;
	}
}
