#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	/**
	 * How a game tells the second disc's story: as the original does, much of it narrated, or expanded, the port's
	 * own documents playing what the original narrates. The player chooses as the second disc begins, where the port
	 * has the expansion; a game that has not been asked, and every save the original game made, has none chosen and
	 * plays the original's.
	 */
	enum class EStoryMode : uint8
	{
		Unchosen = 0,
		Original = 1,
		Expanded = 2,
	};

	/**
	 * The story's progress from the first disc's last scene until the second disc's story first moves it on: a game
	 * still at it has played nothing the two modes tell differently, so it can still be asked.
	 */
	inline constexpr uint16 DiscChangeStoryProgress = 249;
}
