#include "Presentation/WeltallViewportClient.h"

#include "Engine/GameInstance.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "Screens/GameScreen.h"
#include "Screens/GameScreenSubsystem.h"
#include "Shell/PlayerSettingsSubsystem.h"
#include "UnrealClient.h"

void UWeltallViewportClient::Init(FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice)
{
	Super::Init(WorldContext, OwningGameInstance, bCreateNewAudioDevice);
	bDisableWorldRendering = true;
}

void UWeltallViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	// Without world rendering this only clears the viewport and queues the engine's own overlays,
	// which are drawn after the screen.
	Super::Draw(InViewport, SceneCanvas);

	const UGameInstance* Game = GetGameInstance();
	const UGameScreenSubsystem* Screens = Game != nullptr ? Game->GetSubsystem<UGameScreenSubsystem>() : nullptr;
	TSharedPtr<const IScreenFrame> Frame = Screens != nullptr ? Screens->CaptureFrame() : nullptr;
	const UPlayerSettingsSubsystem* Settings = Game != nullptr ? Game->GetSubsystem<UPlayerSettingsSubsystem>() : nullptr;
	const Weltall::FDisplayOptions Options = Settings != nullptr ? Settings->Settings().Display : Weltall::FDisplayOptions{};
	const FIntPoint Size = InViewport->GetSizeXY();
	ENQUEUE_RENDER_COMMAND(DrawGameScreen)([InViewport, Frame = MoveTemp(Frame), Size, Options](FRHICommandListImmediate& RHICmdList)
	{
		FRHITexture* Target = InViewport->GetRenderTargetTexture();
		if (Target == nullptr)
		{
			return;
		}
		FRDGBuilder GraphBuilder(RHICmdList);
		FRDGTextureRef Output = RegisterExternalTexture(GraphBuilder, Target, TEXT("GameViewport"));
		GraphBuilder.SetTextureAccessFinal(Output, ERHIAccess::RTV);
		AddDisplayFramePasses(GraphBuilder, Output, FIntRect(FIntPoint::ZeroValue, Size.ComponentMin(Output->Desc.Extent)), Options, Frame.Get());
		GraphBuilder.Execute();
	});
}
