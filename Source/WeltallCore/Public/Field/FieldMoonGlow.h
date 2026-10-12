#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	class FVram;

	/**
	 * The moon over the Yggdrasil's deck at night (maps 278 and 283) is a picture whose glow is painted wider than the
	 * picture itself: the glow fades out at 80 texels from the moon's middle, the picture's edges lie at 64, so it ends
	 * there in a dark navy the black sky around it shows as a square. This draws the glow's outer part in again, closer
	 * to the moon, from the picture's own texels along each ray from its middle, so that it fades out within the
	 * picture; the moon and the glow's inner part stay as painted. Nothing changes on another map, or where the map's
	 * picture is not the moon's.
	 */
	WELTALLCORE_API void FadeMoonGlow(int32 Map, FVram& Vram);
}
