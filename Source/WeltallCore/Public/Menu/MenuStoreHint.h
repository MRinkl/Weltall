#pragma once

#include "Menu/MenuPrimitive.h"

#include <vector>

namespace Weltall
{
	class FMenuSpriteSheet;

	/**
	 * The File screen's hint of the save stores its two cards do not show, the port's own, for the display to draw
	 * over the menu's frame: above the cards, the menus' L1 and R1 hints (button pictures, shown as the player's
	 * bindings) about an orb for each pair of stores, the shown pair's lit. Appended to Primitives; nothing while
	 * the cards are not shown.
	 */
	WELTALLCORE_API void AddStorePairHint(const FMenuStorePair& Stores, const FMenuSpriteSheet& Sheet, std::vector<FMenuPrimitive>& Primitives);
}
