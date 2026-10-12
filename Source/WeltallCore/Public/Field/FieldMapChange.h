#pragma once

#include "Field/EventContext.h"

namespace Weltall
{
	/**
	 * What the field keeps from one map to the next when its scripts change maps, over the context
	 * a map's load starts afresh (To): what neither the load's reset nor the load itself writes.
	 * - The party's characters, whose sprite files the next map keeps unless it needs the other kind.
	 * - The camera's scripted path (its points, and each move's point and step), its ramps' values
	 *   and steps (only their ticks are reset), the scripted camera's yaw, pitch and distance, the
	 *   follow's and the return's tick counts, the shake's ticks, the view's pitch.
	 * - The encounters' period countdown, the formation last drawn, and the place's weights (a map
	 *   without encounter data keeps them); a battle, the world map, the arena or another program
	 *   still asked for.
	 * - The latched buttons, the message windows' fields a window's last message leaves (the load
	 *   frees the windows themselves) and the count of those opened this tick, the particle
	 *   emitters' setup and FE 8F's and FE 90's words, the field clock, the fade layers' steps, the
	 *   action mode's jump interval, the platforms' touch latch, the menu's parameter and the
	 *   menus pending, the movie's frame and what the last movie asked for, whether the effect
	 *   textures' place holds the picture of the screen, the gears' lights' words the load does not set.
	 * The kernel's sprite ring place is kept too (FFieldRuntime).
	 */
	WELTALLCORE_API void KeepAcrossMaps(const FEventContext& From, FEventContext& To);
}
