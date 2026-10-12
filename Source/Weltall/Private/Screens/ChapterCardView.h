#pragma once

#include "CoreMinimal.h"
#include "Game/ChapterCardDrawing.h"
#include "Screens/GameScreen.h"

class FChapterCardFrame;
class FVramTexture;

namespace Weltall
{
	class FGameSession;
}

/**
 * The title card of the chapter the story has moved into (Weltall::FChapterCard), drawn over the field's
 * frame while it is shown: its band across the whole display, its text on the 320 x 224 screen fitted to it.
 */
class FChapterCardView
{
public:
	FChapterCardView();
	~FChapterCardView();

	/** Background with the session's card over it while one is shown; Background itself otherwise. */
	TSharedRef<const IScreenFrame> Capture(const Weltall::FGameSession& Session, TSharedRef<const IScreenFrame> Background) const;

private:
	mutable Weltall::FChapterCardDrawing Drawing;
	TSharedRef<FVramTexture> Vram;
	/** Frames handed out; one the rendering thread no longer holds is filled again. */
	mutable TArray<TSharedRef<FChapterCardFrame>> DisplayFrames;
};
