#include "Rendering/ScreenBlurRendering.h"

#include "GlobalShader.h"
#include "PixelShaderUtils.h"
#include "RHIStaticStates.h"
#include "RenderGraphBuilder.h"
#include "ShaderParameterStruct.h"

namespace
{
	class FScreenBlurPS final : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FScreenBlurPS);
		SHADER_USE_PARAMETER_STRUCT(FScreenBlurPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Source)
			SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)
			SHADER_PARAMETER(FVector2f, SourceSize)
			SHADER_PARAMETER(FVector4f, AreaUv)
			SHADER_PARAMETER(float, Step)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()
	};
}

IMPLEMENT_GLOBAL_SHADER(FScreenBlurPS, "/Weltall/Private/ScreenBlur.usf", "MainPS", SF_Pixel);

void AddScreenBlurPass(FRDGBuilder& GraphBuilder, FRDGTextureRef Source, FRDGTextureRef Output, const FIntRect& Area, float Step)
{
	if (Area.Width() <= 0 || Area.Height() <= 0)
	{
		return;
	}
	const FVector2f Size(static_cast<float>(Source->Desc.Extent.X), static_cast<float>(Source->Desc.Extent.Y));
	FScreenBlurPS::FParameters* Parameters = GraphBuilder.AllocParameters<FScreenBlurPS::FParameters>();
	Parameters->Source = Source;
	Parameters->LinearClamp = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Parameters->SourceSize = Size;
	Parameters->AreaUv = FVector4f((Area.Min.X + 0.5f) / Size.X, (Area.Min.Y + 0.5f) / Size.Y, (Area.Max.X - 0.5f) / Size.X, (Area.Max.Y - 0.5f) / Size.Y);
	Parameters->Step = Step;
	Parameters->RenderTargets[0] = FRenderTargetBinding(Output, ERenderTargetLoadAction::ELoad);
	const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	const TShaderMapRef<FScreenBlurPS> PixelShader(ShaderMap);
	FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ShaderMap, RDG_EVENT_NAME("ScreenBlur"), PixelShader, Parameters, Area);
}
