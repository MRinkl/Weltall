#include "Rendering/BattlingTrailsRendering.h"

#include "GlobalShader.h"
#include "PixelShaderUtils.h"
#include "RenderGraphBuilder.h"
#include "ShaderParameterStruct.h"

namespace
{
	class FBattlingTrailsPS final : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FBattlingTrailsPS);
		SHADER_USE_PARAMETER_STRUCT(FBattlingTrailsPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Source)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()
	};
}

IMPLEMENT_GLOBAL_SHADER(FBattlingTrailsPS, "/Weltall/Private/BattlingTrails.usf", "MainPS", SF_Pixel);

void AddBattlingTrailsPass(FRDGBuilder& GraphBuilder, FRDGTextureRef Source, FRDGTextureRef Output, const FIntRect& Area)
{
	if (Area.Width() <= 0 || Area.Height() <= 0)
	{
		return;
	}
	FBattlingTrailsPS::FParameters* Parameters = GraphBuilder.AllocParameters<FBattlingTrailsPS::FParameters>();
	Parameters->Source = Source;
	Parameters->RenderTargets[0] = FRenderTargetBinding(Output, ERenderTargetLoadAction::ELoad);
	const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	const TShaderMapRef<FBattlingTrailsPS> PixelShader(ShaderMap);
	FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ShaderMap, RDG_EVENT_NAME("BattlingTrails"), PixelShader, Parameters, Area);
}
