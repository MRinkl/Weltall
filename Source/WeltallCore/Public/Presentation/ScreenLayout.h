#pragma once

#include "Foundation/Types.h"
#include "Presentation/DisplayOptions.h"
#include "Presentation/GameScreen.h"

namespace Weltall
{
	/** A rectangle of whole display pixels. */
	struct FDisplayRect
	{
		int32 X = 0;
		int32 Y = 0;
		int32 Width = 0;
		int32 Height = 0;

		bool operator==(const FDisplayRect&) const = default;
	};

	/** The part of a display the picture takes: the whole of it, or the largest of the aspect's shape in its middle. */
	WELTALLCORE_API FDisplayRect FitAspect(int32 DisplayWidth, int32 DisplayHeight, EDisplayAspect Aspect);

	/**
	 * Where a game screen appears on a display: as large as fits, centred, with square pixels, so a
	 * wider or taller display shows black bars beside or above it.
	 */
	class FScreenLayout
	{
	public:
		WELTALLCORE_API static FScreenLayout Fit(int32 DisplayWidth, int32 DisplayHeight, FScreenSize Screen);

		/** Display pixels per game-screen pixel. */
		double Scale() const { return PixelScale; }

		/** The display pixels a rectangle of the game screen covers, each edge on its nearest pixel boundary. */
		WELTALLCORE_API FDisplayRect ToDisplay(const FScreenRect& Rect) const;

	private:
		double PixelScale = 0.0;
		double OriginX = 0.0;
		double OriginY = 0.0;
	};
}
