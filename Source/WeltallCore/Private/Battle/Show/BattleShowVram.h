#pragma once

#include "Foundation/Types.h"
#include "Gears/GearPackets.h"
#include "Graphics/Vram.h"

#include <span>

namespace Weltall
{
	/**
	 * The three 64 x 256 columns some effects' sprites draw from, which pair with the gears' three texture places: effect E3h's
	 * load copies each place's column over them (bToEffects), system command 6Bh copies them back to the
	 * places.
	 */
	void CopyGearColumns(FVram& Vram, std::span<const FGearTexturePlace> Places, bool bToEffects, FVramImage& Scratch);

	/** LoadImage: the rectangle's 16-bit units, row by row from At in Bytes, into VRAM; false when they run past the bytes. */
	bool UploadBattleUnits(FVram& Vram, std::span<const uint8> Bytes, std::size_t At, const FVramRect& Rect);

	/**
	 * The kernel's image list with its places as the list gives them (mode 0): each image (1100h) and CLUT
	 * (1101h) block at its place plus its offset, until a block of another kind; false when a block runs
	 * past the bytes.
	 */
	bool UploadBattleImageList(FVram& Vram, std::span<const uint8> Bytes, std::size_t At);

	/** MoveImage: a rectangle of VRAM copied to (X, Y), through Scratch (kept from call to call). */
	void MoveBattleImage(FVram& Vram, const FVramRect& Rect, int32 X, int32 Y, FVramImage& Scratch);
}
