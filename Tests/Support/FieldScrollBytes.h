#pragma once

#include "Field/FieldRenderState.h"

#include <vector>

namespace Weltall::Testing
{
	/**
	 * The VRAM scrollers laid out as the event oracles read them from the original's memory: the count,
	 * then per scroller its band
	 * count, speeds, the object's rectangle, band height, bands and destination, and its bands' offsets.
	 */
	std::vector<uint8> LineScrollBytes(const std::vector<FFieldLineScroll>& Scrolls);

	/** The screen wave's words as the original keeps them. */
	std::vector<uint8> ScreenWaveBytes(const FFieldScreenWave& Wave);

	/** The corners of the cells the screen shows, row after row, as 32-bit words. */
	std::vector<uint8> ScreenWaveCellBytes(const FFieldScreenWave& Wave);

	/** The copies of VRAM, each its rectangle and destination as halfwords. */
	std::vector<uint8> VramMoveBytes(const std::vector<FFieldVramOperation>& Moves);
}
