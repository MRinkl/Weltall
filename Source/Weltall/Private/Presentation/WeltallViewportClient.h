#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"

#include "WeltallViewportClient.generated.h"

/**
 * The game's window: it draws the current screen with the port's own renderer instead of a world,
 * every display frame, black around the picture.
 */
UCLASS()
class UWeltallViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:
	virtual void Init(FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice = true) override;
	virtual void Draw(FViewport* InViewport, FCanvas* SceneCanvas) override;
};
