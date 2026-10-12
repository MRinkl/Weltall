#pragma once

#include "Foundation/Types.h"

#include <array>

namespace Weltall
{
	/**
	 * The screen's waver, a cutscene's own drawing over its frame: once the scene is drawn, the frame's middle (Width
	 * pixels from Left of the 320 x 216 screen) is drawn again over itself row by row, its first Rows rows each moved
	 * across by a shift drawn anew every tick within the row's reach about 0.
	 */
	struct FWorldMapScreenWaver
	{
		static constexpr int32 Rows = 192;
		static constexpr int32 Left = 0x40;
		static constexpr int32 Width = 0xC0;
		/** The height of the copy the rows are drawn from: the screen's. */
		static constexpr int32 CopyHeight = 216;

		/** How far each row's shift reaches (the range the draw is taken modulo; 1 keeps the row still). */
		std::array<uint16, Rows> Reach{};
		/** This tick's shifts, in pixels to the right. */
		std::array<int16, Rows> Shifts{};
	};
}
