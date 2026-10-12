#pragma once

#include "CoreMinimal.h"
#include "Media/GameLibrary.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "GameDiscsSubsystem.generated.h"

/** The player's discs, found in the Games folder beside the game when it starts. */
UCLASS()
class WELTALL_API UGameDiscsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The folder the discs are looked for in, and its subfolders: Games beside the game, or the one given as -Games=. */
	static FString GamesFolder();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The disc of a release, or null when the folder has none. */
	const Weltall::FIso9660* Find(Weltall::EDiscRelease Release) const;

	/** Looks in the folder again: discs not found before are added, the ones found stay (the game may be reading them). */
	void Rescan();

	/** What was found, and the notes on the rest. */
	const Weltall::FGameLibrary& Library() const { return Discs; }

private:
	void LogNotes() const;

	Weltall::FGameLibrary Discs;
};
