#pragma once

#include "CoreMinimal.h"

class SBorder;
class STextBlock;

namespace Weltall
{
	struct FError;
}

/** A message the game shows over its viewport: the whole of it, and its text, which can change. */
struct FGameNotice
{
	TSharedRef<SBorder> Widget;
	TSharedRef<STextBlock> Text;
};

/** White text on black in place of the game's picture, or over the picture darkened (bOverPicture). */
WELTALL_API FGameNotice MakeGameNotice(const FText& Text, bool bOverPicture);

/**
 * What the player is told when the game stops, or cannot start (bStarted false): what happened, why, where the log is,
 * and the game's version.
 */
WELTALL_API FText GameStoppedText(const Weltall::FError& Error, bool bStarted);
