#pragma once

#include "CoreMinimal.h"
#include "RenderGraphFwd.h"

/** A rectangle of VRAM that takes pixels of a picture of the screen, a unit a pixel (FieldScreenUnits.usf). */
struct FFieldScreenUnits
{
	/** In VRAM's units. */
	FIntRect Rect;
	/** The screen's pixel (of the 320 x 224 picture) the rectangle's first unit takes. */
	FIntPoint ScreenOffset = FIntPoint::ZeroValue;
	/** The GPU's modulation of the pixels: 128 as shown. */
	uint32 Brightness = 128;
};

/**
 * Adds a pass writing Units' rectangle of Vram (1024 x 512 R16_UINT) from Source, the screen drawn at
 * 320 x 224: each unit the 15-bit colour of its pixel, its mask bit clear; black without a Source.
 */
WELTALL_API void AddFieldScreenUnitsPass(FRDGBuilder& GraphBuilder, FRDGTextureRef Source, FRDGTextureRef Vram, const FFieldScreenUnits& Units);
