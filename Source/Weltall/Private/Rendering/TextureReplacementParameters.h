#pragma once

#include "CoreMinimal.h"
#include "ShaderParameterMacros.h"
#include "TextureReplacementGpu.h"

/** What TextureReplacements.ush reads, in both the 3D scene's and the screen lists' pixel shaders. */
BEGIN_SHADER_PARAMETER_STRUCT(FTextureReplacementParameters, )
	SHADER_PARAMETER_TEXTURE(Texture2D<uint4>, ReplacementViews)
	SHADER_PARAMETER_TEXTURE(Texture2D<uint>, ReplacementRegionMaps)
	SHADER_PARAMETER_TEXTURE(Texture2D<uint4>, ReplacementRegions)
	SHADER_PARAMETER_TEXTURE(Texture2D<uint>, ReplacementPalettes)
	SHADER_PARAMETER_TEXTURE(Texture2D, ReplacementAtlas)
	SHADER_PARAMETER(uint32, ReplacementMask)
	SHADER_PARAMETER(uint32, ReplacementCount)
END_SHADER_PARAMETER_STRUCT()

/** The parameters for resources the caller keeps referenced while the pass draws. Rendering thread. */
inline FTextureReplacementParameters TextureReplacementParameters(const FTextureReplacementResources& Resources)
{
	FTextureReplacementParameters Parameters;
	Parameters.ReplacementViews = Resources.Views;
	Parameters.ReplacementRegionMaps = Resources.RegionMaps;
	Parameters.ReplacementRegions = Resources.Regions;
	Parameters.ReplacementPalettes = Resources.Palettes;
	Parameters.ReplacementAtlas = Resources.Atlas;
	Parameters.ReplacementMask = Resources.ViewMask;
	Parameters.ReplacementCount = Resources.ViewCount;
	return Parameters;
}
