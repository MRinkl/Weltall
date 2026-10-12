#pragma once

#include "Foundation/Types.h"
#include "Graphics/Vram.h"

namespace Weltall
{
	/** What a change the field asks of VRAM, or of the copies of VRAM it keeps, does. */
	enum class EFieldVramChange : uint8
	{
		/** Rect is copied to (ToX, ToY). */
		Copy,
		/** Rect is cleared to black, as the GPU library clears: its width held to 0-1023 and its height to 0-511. */
		Clear,
		/** Rect takes Width x Height units, row by row, from the render state's VramUnits at UnitsAt. */
		Upload,
		/** Rect (the effects' textures) is kept aside, to be put back by RestoreArea. */
		SaveArea,
		/** What SaveArea kept goes back into its rectangle. */
		RestoreArea,
		/**
		 * Rect (whole rows of CLUTs) is kept aside for FadeClutRow, and the rows' working copy starts as
		 * they are (the original's starts as whatever its memory held).
		 */
		SaveClutRows,
		/** Row Row of the CLUT rows kept aside, faded by Factor (FadeClutColors), goes into their working copy. */
		FadeClutRow,
	};

	/** A change of VRAM the field asks for, made by the renderer in the order asked. */
	struct FFieldVramOperation
	{
		EFieldVramChange Kind = EFieldVramChange::Copy;
		FVramRect Rect;
		int16 ToX = 0;
		int16 ToY = 0;
		uint32 UnitsAt = 0;
		int32 Row = 0;
		int32 Factor = 0;
		/** A copy's brightness: its units modulated by it as the GPU modulates a drawn texel (128 leaves them as they are). */
		uint8 Brightness = 128;
	};

	/**
	 * The CLUT rows the scripts fade (FE DD): once kept aside, each fade writes a row of their working
	 * copy, and a tick that faded any loads the whole working copy back where the rows were taken from.
	 */
	struct FFieldClutFade
	{
		/** Kept aside and not given up: rows can be faded. */
		bool bKept = false;
		/** Where the rows were taken from: x 0, 256 units wide. */
		FVramRect Rect;
		/** A row was faded since the last frame: the next frame loads the working copy back. */
		bool bLoadPending = false;
		/**
		 * The tick's frame loaded the working copy into Rect, after the sprites' uploads and the VRAM
		 * scrollers' copies.
		 */
		bool bLoaded = false;
	};
}
