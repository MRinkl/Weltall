#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/GameModeBase.h"

#include "WeltallGameMode.generated.h"

class SBorder;
class STextBlock;

/**
 * The game draws its own screens, so the player has a controller but never a pawn in a world. Play
 * begins as the game does from disc 1: the console powered on, its notice, the opening movie, then
 * the programs as the game's loop runs them. Without disc 1 the player is told where to put it, and
 * the game starts once it is there. A game that stops, or cannot start, says why and where its log is.
 */
UCLASS()
class AWeltallGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWeltallGameMode();

	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;

protected:
	virtual void BeginPlay() override;
	/** The game's screen goes before the sound it plays through. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Powers the game on from disc 1; false when the player's discs have no disc 1. */
	bool StartGame();
	/** Looks in the Games folder again while disc 1 is missing; starts the game once it is there. */
	void LookForFirstDisc();
	/** What the player is told while disc 1 is missing: where it goes, what was found and what was not used. */
	FText FirstDiscMissing() const;
	/** A message over the viewport in place of the game's picture, or over it, darkened (bOverPicture), until hidden. */
	void ShowNotice(const FText& Text, bool bOverPicture);
	void HideNotice();

	/** The game follows the player's settings as they change. */
	FDelegateHandle SettingsChanged;
	FTimerHandle LookAgain;
	TSharedPtr<SBorder> Notice;
	TSharedPtr<STextBlock> NoticeText;
};
