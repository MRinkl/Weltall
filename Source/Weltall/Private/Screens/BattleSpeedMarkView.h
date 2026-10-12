#pragma once

#include "CoreMinimal.h"
#include "Game/BattleSpeedMark.h"
#include "Screens/GameScreen.h"

class FBattleSpeedMarkFrame;
class FVramTexture;

namespace Weltall
{
	class FGameSession;
}

/**
 * The mark of a battle fought faster than the game's pace (Weltall::FBattleSpeedMarkDrawing), drawn in the
 * top right corner of the battle's frame.
 */
class FBattleSpeedMarkView
{
public:
	FBattleSpeedMarkView();
	~FBattleSpeedMarkView();

	/** Background with the mark of the session's battle speed over it; Background itself at the game's own pace. */
	TSharedRef<const IScreenFrame> Capture(const Weltall::FGameSession& Session, TSharedRef<const IScreenFrame> Background) const;

private:
	mutable Weltall::FBattleSpeedMarkDrawing Drawing;
	TSharedRef<FVramTexture> Vram;
	/** Frames handed out; one the rendering thread no longer holds is filled again. */
	mutable TArray<TSharedRef<FBattleSpeedMarkFrame>> DisplayFrames;
};
