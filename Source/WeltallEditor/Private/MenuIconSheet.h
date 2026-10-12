#pragma once

#include "CoreMinimal.h"
#include "Presentation/DrawList.h"
#include "Screens/GameScreen.h"

class FVramTexture;

/**
 * The port's button pictures laid out on the menus' 320 x 224 screen, for review (-Screen=MenuIcons):
 * for each icon set (Xbox, PlayStation, Nintendo, Steam Deck, the keyboard) a row of its icons in a
 * face button's slot, then a row of them in the L1/R1 hints' box, over the menus' dark window grey.
 */
class FMenuIconSheetScreen final : public IGameScreen
{
public:
	FMenuIconSheetScreen();

	virtual void Update(double DeltaSeconds, Weltall::FPadState Held) override {}
	virtual TSharedRef<const IScreenFrame> CaptureFrame() const override;

private:
	TSharedRef<FVramTexture> Pictures;
	Weltall::FDrawList List;
};
