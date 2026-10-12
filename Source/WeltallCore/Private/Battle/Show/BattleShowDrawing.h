#pragma once

#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/Sprite.h"

#include <array>

namespace Weltall::BattleShowDrawing
{
	/** The ordering table's last entry, and how many bits of a depth / 4 the battle drops for an entry. */
	inline constexpr uint32 LastEntry = 0xFFF;
	inline constexpr int32 EntryShift = 2;

	/** A sprite's place in whole units. */
	inline FGteVector Place(const FSprite& Sprite)
	{
		return {static_cast<int16>(Sprite.Position[0] >> 16), static_cast<int16>(Sprite.Position[1] >> 16), static_cast<int16>(Sprite.Position[2] >> 16)};
	}

	/** A sprite's entry in the ordering table from its place's depth under View and its bias; 0 when the GTE flags the depth. */
	inline int32 SpriteEntry(FGte& Gte, const FSprite& Sprite, const FGteMatrix& View)
	{
		constexpr uint32 DepthOverflowFlag = 0x8000;
		Gte.Transform = View;
		const FGteProjection Point = Gte.Rtps(Place(Sprite));
		if ((Point.Flag & DepthOverflowFlag) != 0)
		{
			return 0;
		}
		return (Point.ScreenZ >> 2 >> EntryShift) + Sprite.OtBias;
	}

	/** Whether the ordering table links a drawing into Entry: any but its first. */
	inline bool InTable(int32 Entry)
	{
		return static_cast<uint32>(Entry - 1) < LastEntry;
	}

	/** The sprite's matrix: the kernel builds the transform's matrix again once a setter has marked it. */
	inline void UpdateMatrix(FSprite& Sprite)
	{
		Sprite.DrawFlags &= ~SpriteDrawFlags::MatrixDirty;
	}

	/** A packet's first word: its colour with the GPU code above it. */
	inline uint32 ColorCode(const std::array<uint8, 3>& Color, uint8 Code)
	{
		return static_cast<uint32>(Color[0]) | static_cast<uint32>(Color[1]) << 8 | static_cast<uint32>(Color[2]) << 16 | static_cast<uint32>(Code) << 24;
	}
}
