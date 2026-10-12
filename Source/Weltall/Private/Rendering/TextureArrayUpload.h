#pragma once

#include "CoreMinimal.h"
#include "RHIResources.h"

/** A texture array of square RGBA8 layers, one per entry of Layers; an empty set still gets one blank layer. */
FTextureRHIRef CreateLayeredTexture(FRHICommandListBase& RHICmdList, const TCHAR* Name, int32 LayerSize, const TArray<TArray<uint8>>& Layers);
