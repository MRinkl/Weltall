#pragma once

#include "GlobalShader.h"
#include "RenderGraphResources.h"
#include "ShaderParameterStruct.h"
#include "ShaderPermutation.h"
#include "TextureReplacementParameters.h"

/** A vertex as the draw-list shader reads it; the layout matches FScreenVertex in ScreenDrawList.usf. */
struct FScreenDrawVertex
{
	FVector2f Position;
	FVector2f TexCoord;
	FVector4f Color;
	FVector4f Clip;
	uint32 Texture = 0;
	uint32 Flags = 0;
	/** Drawing from VRAM, the texture's page and CLUT as FVramTexture packs them. */
	uint32 VramTexture = 0;
	/** The primitive's texels, first and last (u, v, u, v a byte each): a pixel on its very edge still takes one of its own. */
	uint32 TexelBounds = 0;
};
static_assert(sizeof(FScreenDrawVertex) == 64, "the draw-list shader reads 64-byte vertices");

class FScreenDrawListVS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FScreenDrawListVS);
	SHADER_USE_PARAMETER_STRUCT(FScreenDrawListVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FScreenVertex>, Vertices)
		SHADER_PARAMETER(FVector4f, ScreenRect)
		SHADER_PARAMETER(FVector2f, ScreenSize)
		SHADER_PARAMETER(FVector2f, TargetSize)
		SHADER_PARAMETER(uint32, FirstVertex)
	END_SHADER_PARAMETER_STRUCT()
};

class FScreenDrawListPS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FScreenDrawListPS);
	SHADER_USE_PARAMETER_STRUCT(FScreenDrawListPS, FGlobalShader);

	/** Textures fetched from VRAM through their pages and CLUTs, rather than from decoded layers. */
	class FRawVram : SHADER_PERMUTATION_BOOL("RAW_VRAM");
	using FPermutationDomain = TShaderPermutationDomain<FRawVram>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2DArray, Textures)
		SHADER_PARAMETER_TEXTURE(Texture2D<uint>, Vram)
		SHADER_PARAMETER_STRUCT_INCLUDE(FTextureReplacementParameters, Replacements)
		SHADER_PARAMETER(FVector4f, ScreenRect)
		SHADER_PARAMETER(FVector2f, ScreenSize)
		SHADER_PARAMETER(uint32, DrawPass)
		SHADER_PARAMETER(float, ColorScale)
		SHADER_PARAMETER(float, OutputAlpha)
	END_SHADER_PARAMETER_STRUCT()
};
