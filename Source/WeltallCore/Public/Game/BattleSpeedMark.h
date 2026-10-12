#pragma once

#include "Foundation/Types.h"
#include "Game/BattleSpeed.h"
#include "Graphics/Vram.h"
#include "Presentation/DrawList.h"
#include "Presentation/GameScreen.h"
#include "Presentation/ScreenLayout.h"

#include <string_view>

namespace Weltall
{
	class FMesFont;

	/** The screen the battle speed's mark is laid out on, in lines of the game's 224-line screen. */
	inline constexpr FScreenSize BattleSpeedMarkScreen{24, 24};

	/**
	 * The mark shown while a battle is fought faster than the game's pace: two arrowheads pointing right, white
	 * inside a dark edge that keeps them legible over any stage, and under them the speed ("2x", "4x") in the
	 * game's message font with its outline, its letters side by side, both ending at the screen's right edge.
	 */
	class FBattleSpeedMarkDrawing
	{
	public:
		/**
		 * The mark for Speed as a list over BattleSpeedMarkScreen, its text in Pictures(), where it is drawn
		 * when the speed asked for changes. Nothing at the game's own pace.
		 */
		WELTALLCORE_API void Build(EBattleSpeed Speed, const FMesFont& Font, FDrawList& Out);

		const FVram& Pictures() const { return PictureVram; }

	private:
		void DrawText(std::string_view Text, const FMesFont& Font);

		FVram PictureVram;
		EBattleSpeed Drawn = EBattleSpeed::Normal;
		/** The columns of the picture the text's pixels reach. */
		int32 TextWidth = 0;
	};

	/**
	 * Where the mark's screen goes in a picture of Width by Height display pixels: in its top right corner,
	 * five of the game screen's lines from the edges. On the game's own 4:3 screen that is the strip beside
	 * a third party member's panel, which the battle draws nothing in; a wider picture has it clear of them.
	 */
	WELTALLCORE_API FDisplayRect PlaceBattleSpeedMark(int32 Width, int32 Height);
}
