#pragma once

#include "Foundation/Types.h"

#include <compare>
#include <vector>

namespace Weltall
{
	/** A size in display pixels, ordered by width, then height. */
	struct FDisplaySize
	{
		int32 Width = 0;
		int32 Height = 0;

		bool IsSet() const { return Width > 0 && Height > 0; }
		auto operator<=>(const FDisplaySize&) const = default;
	};

	/** The shape of the picture in the window: the window's own, or the widest of a fixed shape that fits, bars around it. */
	enum class EDisplayAspect : uint8
	{
		Window,
		Wide,
		Standard,
	};

	/** How the player has the game's window. */
	struct FDisplayOptions
	{
		/** The window's size the player chose; unset, the window is as the platform opens it. */
		FDisplaySize WindowSize;
		EDisplayAspect Aspect = EDisplayAspect::Window;
		/** The game stops, its play time with it, while its window is not the one in use. */
		bool bPauseInBackground = true;
		/** The sound is heard while the window is not the one in use (and the game goes on). */
		bool bSoundInBackground = false;
		/** The game fills the display without a border (never the display's own full-screen mode); off, a window of WindowSize. */
		bool bFullScreen = false;

		bool operator==(const FDisplayOptions&) const = default;
	};

	/** What the display offers the options screen: the sizes a window can take, smallest first, and the window's now. None where the game has no window. */
	struct FDisplayModes
	{
		std::vector<FDisplaySize> WindowSizes;
		FDisplaySize Window;
	};
}
