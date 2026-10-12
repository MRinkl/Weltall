#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	struct FEventContext;

	/**
	 * Turns an angle (4096 a turn) by Speed toward Target the shorter way, landing on the target
	 * rather than passing it; the result is 0 to 4095.
	 */
	WELTALLCORE_API int32 StepFieldAngle(int32 Angle, int32 Target, int32 Speed);

	/**
	 * The actors' turning after the camera's tick: every script actor that takes part, is not removed
	 * and is not frozen off screen turns its shown yaw toward its facing at its turn speed (a gear's
	 * at the gears' speed; on a ladder, toward the ladder's direction), unless its facing is locked;
	 * then its sprite faces the shown yaw relative to the view, or, without animations, moves along
	 * it.
	 */
	WELTALLCORE_API void TurnFieldActors(FEventContext& Context);
}
