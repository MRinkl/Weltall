#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"

#include "ExportTexturesCommandlet.generated.h"

/**
 * Runs the Assets menu's texture export (FTextureExport) on the first disc in Games into a textures folder, headless,
 * and logs how many items it took and how long.
 *
 *   UnrealEditor-Cmd Weltall.uproject -run=ExportTextures -Out=<folder> -unattended -nullrhi
 */
UCLASS()
class UExportTexturesCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	UExportTexturesCommandlet();

	virtual int32 Main(const FString& Params) override;
};
