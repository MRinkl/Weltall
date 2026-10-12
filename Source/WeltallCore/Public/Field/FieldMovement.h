#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	/**
	 * What the field's movement keeps for the whole map rather than per actor: the jump's animation
	 * and the player's action mode, how the pushing floors push, and whether the actors have taken
	 * their floors yet. Entering a map starts it afresh.
	 */
	struct FFieldMovement
	{
		static constexpr int16 DefaultJumpAnimation = 3;
		static constexpr int16 UnitPushScale = 0x1000;

		/** The animation a jump plays; playing any other ends the jump. */
		int16 JumpAnimation = DefaultJumpAnimation;
		/**
		 * Non-zero in the player's action mode: a press of the jump button jumps, even in the air,
		 * whenever JumpCooldown is out; each jump reloads it with JumpInterval and each tick of the pad's
		 * control counts it down. Outside the mode, only animation 3 keeps a jump going.
		 */
		int16 ActionMode = 0;
		int16 JumpInterval = 0;
		int16 JumpCooldown = 0;
		/** Added to every pushing floor's direction, 4096 a turn. */
		uint16 FloorPushAngle = 0;
		/** Scales every pushing floor's speed, UnitPushScale = 1. */
		int16 FloorPushScale = UnitPushScale;
		/**
		 * Set once the actors have been moved on the map's first tick: until then no actor counts as
		 * settled, so every one of them takes its floor.
		 */
		bool bFirstTickDone = false;
	};
}
