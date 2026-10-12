#include "Rendering/FieldPictureRendering.h"

#include "CommonRenderResources.h"
#include "FieldPictureShaders.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

IMPLEMENT_GLOBAL_SHADER(FFieldPictureVS, "/Weltall/Private/FieldPicture.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FFieldPicturePS, "/Weltall/Private/FieldPicture.usf", "MainPS", SF_Pixel);

namespace
{
	BEGIN_SHADER_PARAMETER_STRUCT(FFieldPicturePassParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, Vertices)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float>, Shades)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Source)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
}

void AddFieldPicturePass(FRDGBuilder& GraphBuilder, FRDGTextureRef Source, FRDGTextureRef Output, const FFieldPictureDraw& Picture)
{
	if (Picture.Vertices.IsEmpty())
	{
		return;
	}
	const uint32 Count = static_cast<uint32>(Picture.Vertices.Num());
	FVector4f* Vertices = GraphBuilder.AllocPODArray<FVector4f>(Count);
	FMemory::Memcpy(Vertices, Picture.Vertices.GetData(), Count * sizeof(FVector4f));
	FRDGBufferRef Buffer = CreateStructuredBuffer(GraphBuilder, TEXT("FieldPictureVertices"), sizeof(FVector4f), Count, Vertices, Count * sizeof(FVector4f),
		ERDGInitialDataFlags::NoCopy);
	// A picture without shades still binds one.
	const bool bShaded = Picture.Shades.Num() == Picture.Vertices.Num();
	const uint32 ShadeCount = bShaded ? Count : 1;
	float* Shades = GraphBuilder.AllocPODArray<float>(ShadeCount);
	Shades[0] = 1.0f;
	if (bShaded)
	{
		FMemory::Memcpy(Shades, Picture.Shades.GetData(), ShadeCount * sizeof(float));
	}
	FRDGBufferRef ShadeBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("FieldPictureShades"), sizeof(float), ShadeCount, Shades, ShadeCount * sizeof(float),
		ERDGInitialDataFlags::NoCopy);
	FFieldPicturePassParameters* PassParameters = GraphBuilder.AllocParameters<FFieldPicturePassParameters>();
	PassParameters->Vertices = GraphBuilder.CreateSRV(Buffer);
	PassParameters->Shades = GraphBuilder.CreateSRV(ShadeBuffer);
	PassParameters->Source = Source;
	PassParameters->RenderTargets[0] = FRenderTargetBinding(Output, Picture.bClearFirst ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad);

	const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FFieldPictureVS> VertexShader(ShaderMap);
	TShaderMapRef<FFieldPicturePS> PixelShader(ShaderMap);
	const FIntPoint TargetSize = Output->Desc.Extent;
	GraphBuilder.AddPass(RDG_EVENT_NAME("FieldPicture"), PassParameters, ERDGPassFlags::Raster,
		[PassParameters, VertexShader, PixelShader, TargetSize, Count, bShaded, Placement = Picture.Placement, SourcePlacement = Picture.SourcePlacement,
			ColorScale = Picture.ColorScale, Blend = Picture.Blend, bScissor = Picture.bScissor, Scissor = Picture.Scissor](FRDGAsyncTask, FRHICommandList& RHICmdList)
		{
			RHICmdList.SetViewport(0.0f, 0.0f, 0.0f, static_cast<float>(TargetSize.X), static_cast<float>(TargetSize.Y), 1.0f);
			if (bScissor)
			{
				RHICmdList.SetScissorRect(true, Scissor.Min.X, Scissor.Min.Y, Scissor.Max.X, Scissor.Max.Y);
			}
			FGraphicsPipelineStateInitializer State;
			RHICmdList.ApplyCachedRenderTargets(State);
			State.BlendState = Blend == EFieldPictureBlend::Add ? TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI()
				: Blend == EFieldPictureBlend::Average ? TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI()
				: TStaticBlendState<CW_RGB>::GetRHI();
			State.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			State.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			State.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			State.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			State.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			State.PrimitiveType = PT_TriangleList;
			SetGraphicsPipelineState(RHICmdList, State, 0);
			FFieldPictureVS::FParameters VertexParameters;
			VertexParameters.Vertices = PassParameters->Vertices;
			VertexParameters.Placement = Placement;
			VertexParameters.SourcePlacement = SourcePlacement;
			VertexParameters.TargetSize = FVector2f(TargetSize.X, TargetSize.Y);
			VertexParameters.Shades = PassParameters->Shades;
			VertexParameters.bShaded = bShaded ? 1u : 0u;
			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), VertexParameters);
			FFieldPicturePS::FParameters PixelParameters;
			PixelParameters.Source = PassParameters->Source;
			PixelParameters.SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			PixelParameters.ColorScale = ColorScale;
			PixelParameters.bBlackIsTransparent = Blend == EFieldPictureBlend::OpaqueBlackTransparent ? 1u : 0u;
			PixelParameters.OutputAlpha = Blend == EFieldPictureBlend::Average ? 0.5f : 1.0f;
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PixelParameters);
			RHICmdList.DrawPrimitive(0, Count / 3, 1);
		});
}
