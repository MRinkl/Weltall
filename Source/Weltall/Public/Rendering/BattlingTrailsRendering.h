#pragma once

#include "CoreMinimal.h"
#include "RenderGraphFwd.h"

/**
 * Adds a pass drawing Area of Source into the same place of Output as the arena's demonstration copies its frame before
 * under the next: each pixel's 5-bit channels at 70h/80h of their value, the black pixels left out, Output keeping what it
 * holds there (BattlingTrails.usf).
 */
WELTALL_API void AddBattlingTrailsPass(FRDGBuilder& GraphBuilder, FRDGTextureRef Source, FRDGTextureRef Output, const FIntRect& Area);
