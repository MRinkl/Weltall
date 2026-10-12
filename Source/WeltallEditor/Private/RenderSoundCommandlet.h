#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"

#include "RenderSoundCommandlet.generated.h"

/**
 * Renders the game's sound into a WAV file (44.1 kHz, 16-bit stereo), without a window or an audio
 * device: a field song as the field plays it, or an effect of effect file 0 as a field script plays
 * it, from the game's start on, for some seconds.
 *
 *   UnrealEditor-Cmd Weltall.uproject -run=RenderSound -Song=N|-Effect=N -Seconds=S -Out=path.wav
 *       -unattended -nullrhi
 */
UCLASS()
class URenderSoundCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	URenderSoundCommandlet();

	virtual int32 Main(const FString& Params) override;
};
