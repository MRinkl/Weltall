#pragma once

#include "Graphics/Vram.h"

namespace Weltall::BattleHudLists
{
	inline constexpr int32 FullScale = 0x1000;
	inline constexpr uint8 FullLevel = 0x80;
	inline constexpr int16 TextRows = 13;
	/** The narrow text rows (1Bh units typed, 1Eh wide), the wide one (39h typed, 3Ch wide) and a digit's image. */
	inline constexpr int16 NarrowUnits = 0x1E;
	inline constexpr int16 WideUnits = 0x3C;
	inline constexpr int16 DigitUnits = 6;
	/** The wide row the lists type their help into, and the texels its quads start from. */
	inline constexpr int16 HelpX = 0x3C0;
	/** Where the labels' rows and the digits' images begin in the page at (3C0h, 100h): u 78h (unit 3DEh), a digit 8 texels on from another. */
	inline constexpr uint8 LabelsU = 0x78;
	/** The battle labels the lists draw: a kind (7 all, 8 one, 9 a gear's), targets (Ch one, Dh all, 15h the party). */
	inline constexpr int32 AllKindLabel = 7;
	inline constexpr int32 OneKindLabel = 8;
	inline constexpr int32 GearKindLabel = 9;
	inline constexpr int32 OneTargetLabel = 0xC;
	inline constexpr int32 AllTargetsLabel = 0xD;
	inline constexpr int32 PartyTargetLabel = 0x15;
	/** The list's sprites: the scroll bar's thumb piece. */
	inline constexpr int32 ThumbFrame = 0x66;

	/** A text row as the lists allocate one, cleared. */
	inline FVramImage BlankImage(int16 Units, int16 Rows = TextRows)
	{
		FVramImage Image;
		Image.Rect = {0, 0, Units, Rows};
		Image.Units.assign(static_cast<std::size_t>(Units) * static_cast<std::size_t>(Rows), 0);
		return Image;
	}
}
