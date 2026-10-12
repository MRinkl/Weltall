#pragma once

#include "Field/FieldRenderState.h"

#include <vector>

namespace Weltall::Testing
{
	/**
	 * The scripts' screen pieces of a tick as the tick oracle records them:
	 * the uploads FE 77 and FE B1 made ('U', the rectangle and its units), FE DD's rows kept aside ('D',
	 * the rectangle) and faded ('F', the row and the factor), the frame's load of the faded rows ('L', the
	 * rectangle), then what the frame draws: each placed marker's SPRT ('M') and each of the cockpit's
	 * POLY_FT4s ('H') in drawing order, and the cockpit's counter and light ('C'). Empty when there is none.
	 * The cockpit's quads need its layout.
	 */
	std::vector<uint8> ScreenPieceBytes(const FFieldRenderState& Render, const FFieldCockpitHudLayout* Layout);
}
