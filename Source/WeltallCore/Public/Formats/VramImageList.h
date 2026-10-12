#pragma once

#include "Foundation/Result.h"
#include "Graphics/Vram.h"

#include <span>
#include <vector>

namespace Weltall
{
	/**
	 * The uploads of the kernel's image list as the kernel makes them with the images and CLUTs at
	 * their own places: a count, as many words, then blocks (a kind, 1100h pixels or 1101h a CLUT; a place, an
	 * offset from it, the width and height; the units), each uploaded at its place plus its offset. The loader
	 * stops at a block of another kind.
	 */
	WELTALLCORE_API TResult<std::vector<FVramImage>> ReadVramImageList(std::span<const uint8> List);
}
