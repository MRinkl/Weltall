#pragma once

#include "Foundation/Types.h"

#include <span>
#include <string_view>

namespace Weltall::MenuIconFont
{
	/** A character of the icons' fonts: its rows, '#' for a set texel; every row as wide. */
	struct FGlyph
	{
		char Code = 0;
		std::span<const std::string_view> Rows;

		int32 Width() const { return Rows.empty() ? 0 : static_cast<int32>(Rows[0].size()); }
		int32 Height() const { return static_cast<int32>(Rows.size()); }
	};

	/**
	 * The labels' font, 5 rows high: capitals, digits, the keys' punctuation, the arrows as 'u', 'd', 'l' and
	 * 'r', the keys' symbols as 'e' (Enter), 's' (Space), 'b' (Backspace), 't' (Tab), 'h' (Shift) and 'k'
	 * (Command), and the mouse as 'm', with its left, right or middle button pressed as 'x', 'y' and 'z'.
	 */
	FGlyph SmallGlyph(char Code);

	/** The face buttons' letters, 7 rows high: A, B, X and Y. */
	FGlyph BigGlyph(char Code);
}
