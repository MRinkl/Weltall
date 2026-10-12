#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/Vram.h"

#include <span>
#include <vector>

namespace Weltall
{
	/**
	 * The uploads of a VRAM stream, the kernel's format for images it copies into VRAM sector by
	 * sector as the drive delivers them. A stream is blocks of a header sector (u32 1200h for an
	 * image, 1201h for CLUTs; s16 x, y, dx, dy, width; the stream's block count at +20, taken from
	 * the first header; the block's data sector count at +24; each data sector's rows as s16 from
	 * +28) followed by its data sectors. Each data sector is one upload at (x + dx, y + dy), below the
	 * rows the block's earlier sectors filled: the placement the field asks for.
	 *
	 * The kernel stops after the stream's last block, at a header of another kind, or at the end of
	 * the file, whichever comes first.
	 */
	WELTALLCORE_API TResult<std::vector<FVramImage>> DecodeVramStream(std::span<const uint8> Sectors);
}
