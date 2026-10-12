#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	/** The size of the picture a game screen shows, in its own pixels (the GPU's display area). */
	struct FScreenSize
	{
		int32 Width = 0;
		int32 Height = 0;

		bool operator==(const FScreenSize&) const = default;
	};

	/** A rectangle of the game screen, in its pixels. */
	struct FScreenRect
	{
		int16 X = 0;
		int16 Y = 0;
		int16 Width = 0;
		int16 Height = 0;

		bool operator==(const FScreenRect&) const = default;
	};
}
