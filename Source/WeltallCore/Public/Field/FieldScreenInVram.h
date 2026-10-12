#pragma once

#include "Foundation/Types.h"
#include "Graphics/Vram.h"

#include <span>
#include <vector>

namespace Weltall
{
	struct FFieldCover;
	struct FFieldVramOperation;

	/**
	 * A rectangle of VRAM holding pixels of a picture of the screen, which the field copied there from
	 * the picture of the screen's area while that held the screen (transition 5's move, the entry
	 * after a defeat). The core's VRAM holds no screen: the renderer puts the pixels there, a unit a
	 * pixel of the 320 x 224 screen.
	 */
	struct FFieldScreenInVram
	{
		FVramRect Rect;
		/** The screen's pixel the rectangle's first unit holds. */
		int16 ScreenX = 0;
		int16 ScreenY = 0;
		/** The brightness the pixels were drawn at before the copy, as the GPU modulates them: 128 as shown. */
		uint8 Brightness = 128;
		/** Which picture: the count of pictures taken when it was taken (FFieldCover::Taken). */
		int32 Taken = 0;

		bool operator==(const FFieldScreenInVram&) const = default;
	};

	/** What is left of the rectangles once Written has been written over (wrapping at VRAM's edges, as an upload does). */
	WELTALLCORE_API void WriteOverScreenInVram(std::vector<FFieldScreenInVram>& Regions, const FVramRect& Written);

	/**
	 * The screen's pixels a copy of VRAM (an EFieldVramChange::Copy) carries to its destination, into
	 * Carried: those of the rectangles holding them, and those of the picture of the screen's area
	 * while it holds the screen (Cover.bScreen).
	 */
	WELTALLCORE_API void CarryScreenInVram(std::span<const FFieldScreenInVram> Regions, const FFieldVramOperation& Copy, const FFieldCover& Cover,
		std::vector<FFieldScreenInVram>& Carried);
}
