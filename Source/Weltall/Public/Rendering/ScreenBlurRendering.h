#pragma once

#include "CoreMinimal.h"
#include "RenderGraphFwd.h"

/**
 * Adds a pass drawing Area of Source into the same place of Output blurred once as the PlayStation's 2 x 2 means blur a
 * picture: each pixel the mean of itself and the pixels Step pixels (one of the original screen's) to its right, below it,
 * and both, the taps kept inside Area (ScreenBlur.usf).
 */
WELTALL_API void AddScreenBlurPass(FRDGBuilder& GraphBuilder, FRDGTextureRef Source, FRDGTextureRef Output, const FIntRect& Area, float Step);
