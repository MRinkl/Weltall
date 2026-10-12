#pragma once

#include "CoreMinimal.h"
#include "Game/SaveStore.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include <optional>

#include "SaveStoreSubsystem.generated.h"

/** The player's saves: the eight stores the Menu's File screen shows, in the player's saves folder. */
UCLASS()
class WELTALL_API USaveStoreSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	Weltall::FSaveStore& Store() { return *Saves; }

private:
	std::optional<Weltall::FSaveStore> Saves;
};
