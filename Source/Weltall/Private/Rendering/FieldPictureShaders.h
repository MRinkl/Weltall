#pragma once

#include "GlobalShader.h"
#include "RenderGraphResources.h"
#include "ShaderParameterStruct.h"

/** The shaders of FieldPicture.usf: a picture drawn at the display's resolution, drawn again over quads. */
class FFieldPictureVS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFieldPictureVS);
	SHADER_USE_PARAMETER_STRUCT(FFieldPictureVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, Vertices)
		SHADER_PARAMETER(FVector4f, Placement)
		SHADER_PARAMETER(FVector4f, SourcePlacement)
		SHADER_PARAMETER(FVector2f, TargetSize)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float>, Shades)
		SHADER_PARAMETER(uint32, bShaded)
	END_SHADER_PARAMETER_STRUCT()
};

class FFieldPicturePS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFieldPicturePS);
	SHADER_USE_PARAMETER_STRUCT(FFieldPicturePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Source)
		SHADER_PARAMETER_SAMPLER(SamplerState, SourceSampler)
		SHADER_PARAMETER(float, ColorScale)
		SHADER_PARAMETER(uint32, bBlackIsTransparent)
		SHADER_PARAMETER(float, OutputAlpha)
	END_SHADER_PARAMETER_STRUCT()
};
