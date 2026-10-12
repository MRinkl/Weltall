#include "Presentation/ScreenLayout.h"

#include <algorithm>
#include <cmath>

namespace Weltall
{
	namespace
	{
		int32 NearestBoundary(double Position)
		{
			return static_cast<int32>(std::lround(Position));
		}
	}

	FDisplayRect FitAspect(int32 DisplayWidth, int32 DisplayHeight, EDisplayAspect Aspect)
	{
		if (Aspect == EDisplayAspect::Window || DisplayWidth <= 0 || DisplayHeight <= 0)
		{
			return {0, 0, std::max(DisplayWidth, 0), std::max(DisplayHeight, 0)};
		}
		const int32 Across = Aspect == EDisplayAspect::Wide ? 16 : 4;
		const int32 Down = Aspect == EDisplayAspect::Wide ? 9 : 3;
		const int32 Width = std::min(DisplayWidth, static_cast<int32>(static_cast<int64>(DisplayHeight) * Across / Down));
		const int32 Height = std::min(DisplayHeight, static_cast<int32>(static_cast<int64>(DisplayWidth) * Down / Across));
		return {(DisplayWidth - Width) / 2, (DisplayHeight - Height) / 2, Width, Height};
	}

	FScreenLayout FScreenLayout::Fit(int32 DisplayWidth, int32 DisplayHeight, FScreenSize Screen)
	{
		FScreenLayout Layout;
		if (DisplayWidth <= 0 || DisplayHeight <= 0 || Screen.Width <= 0 || Screen.Height <= 0)
		{
			return Layout;
		}
		Layout.PixelScale = std::min(static_cast<double>(DisplayWidth) / Screen.Width, static_cast<double>(DisplayHeight) / Screen.Height);
		Layout.OriginX = (DisplayWidth - Screen.Width * Layout.PixelScale) / 2.0;
		Layout.OriginY = (DisplayHeight - Screen.Height * Layout.PixelScale) / 2.0;
		return Layout;
	}

	FDisplayRect FScreenLayout::ToDisplay(const FScreenRect& Rect) const
	{
		const int32 Left = NearestBoundary(OriginX + Rect.X * PixelScale);
		const int32 Top = NearestBoundary(OriginY + Rect.Y * PixelScale);
		const int32 Right = NearestBoundary(OriginX + (Rect.X + Rect.Width) * PixelScale);
		const int32 Bottom = NearestBoundary(OriginY + (Rect.Y + Rect.Height) * PixelScale);
		return {Left, Top, Right - Left, Bottom - Top};
	}
}
