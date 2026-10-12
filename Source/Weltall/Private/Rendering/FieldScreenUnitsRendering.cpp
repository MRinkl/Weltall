#include "Rendering/FieldScreenUnitsRendering.h"

#include "CommonRenderResources.h"
#include "GlobalShader.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"

/** The shaders of FieldScreenUnits.usf: a picture of the screen written into VRAM's units. */
class FFieldScreenUnitsVS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFieldScreenUnitsVS);
	SHADER_USE_PARAMETER_STRUCT(FFieldScreenUnitsVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector4f, Rect)
		SHADER_PARAMETER(FVector2f, VramSize)
	END_SHADER_PARAMETER_STRUCT()
};

class FFieldScreenUnitsPS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFieldScreenUnitsPS);
	SHADER_USE_PARAMETER_STRUCT(FFieldScreenUnitsPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector4f, Rect)
		SHADER_PARAMETER(FIntPoint, ScreenOffset)
		SHADER_PARAMETER(uint32, Brightness)
		SHADER_PARAMETER(uint32, bHasSource)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Source)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_R16_UINT);
	}
};

IMPLEMENT_GLOBAL_SHADER(FFieldScreenUnitsVS, "/Weltall/Private/FieldScreenUnits.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FFieldScreenUnitsPS, "/Weltall/Private/FieldScreenUnits.usf", "MainPS", SF_Pixel);

namespace
{
	BEGIN_SHADER_PARAMETER_STRUCT(FFieldScreenUnitsPassParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Source)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
}

void AddFieldScreenUnitsPass(FRDGBuilder& GraphBuilder, FRDGTextureRef Source, FRDGTextureRef Vram, const FFieldScreenUnits& Units)
{
	// A pass needs a texture to read: without a picture the units are black, a texel of black never read.
	FRDGTextureRef Read = Source;
	if (Read == nullptr)
	{
		Read = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(FIntPoint(1, 1), PF_B8G8R8A8, FClearValueBinding::Black,
			TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("FieldScreenUnitsBlack"));
		AddClearRenderTargetPass(GraphBuilder, Read, FLinearColor::Black);
	}
	FFieldScreenUnitsPassParameters* PassParameters = GraphBuilder.AllocParameters<FFieldScreenUnitsPassParameters>();
	PassParameters->Source = Read;
	PassParameters->RenderTargets[0] = FRenderTargetBinding(Vram, ERenderTargetLoadAction::ELoad);
	const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FFieldScreenUnitsVS> VertexShader(ShaderMap);
	TShaderMapRef<FFieldScreenUnitsPS> PixelShader(ShaderMap);
	const FIntPoint VramSize = Vram->Desc.Extent;
	const bool bHasSource = Source != nullptr;
	GraphBuilder.AddPass(RDG_EVENT_NAME("FieldScreenUnits"), PassParameters, ERDGPassFlags::Raster,
		[PassParameters, VertexShader, PixelShader, VramSize, Units, bHasSource](FRDGAsyncTask, FRHICommandList& RHICmdList)
		{
			RHICmdList.SetViewport(0.0f, 0.0f, 0.0f, static_cast<float>(VramSize.X), static_cast<float>(VramSize.Y), 1.0f);
			FGraphicsPipelineStateInitializer State;
			RHICmdList.ApplyCachedRenderTargets(State);
			State.BlendState = TStaticBlendState<>::GetRHI();
			State.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			State.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			State.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			State.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			State.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			State.PrimitiveType = PT_TriangleList;
			SetGraphicsPipelineState(RHICmdList, State, 0);
			const FVector4f Rect(Units.Rect.Min.X, Units.Rect.Min.Y, Units.Rect.Width(), Units.Rect.Height());
			FFieldScreenUnitsVS::FParameters VertexParameters;
			VertexParameters.Rect = Rect;
			VertexParameters.VramSize = FVector2f(VramSize.X, VramSize.Y);
			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), VertexParameters);
			FFieldScreenUnitsPS::FParameters PixelParameters;
			PixelParameters.Rect = Rect;
			PixelParameters.ScreenOffset = Units.ScreenOffset;
			PixelParameters.Brightness = Units.Brightness;
			PixelParameters.bHasSource = bHasSource ? 1u : 0u;
			PixelParameters.Source = PassParameters->Source;
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PixelParameters);
			RHICmdList.DrawPrimitive(0, 2, 1);
		});
}
