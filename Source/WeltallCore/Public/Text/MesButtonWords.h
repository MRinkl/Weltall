#pragma once

#include "Foundation/Types.h"
#include "Input/PadState.h"
#include "Text/MesWindow.h"

#include <span>
#include <vector>

namespace Weltall
{
	/**
	 * A button the English text names in words ("the R1 button", "the L2 and R2 buttons", "the Directional button"): the
	 * glyphs of its name in the line, where the name ends (the units of the glyph after it), and the button. The
	 * directional buttons are one name: Button is Up and bDirections set.
	 */
	struct FMesButtonWord
	{
		std::size_t FirstGlyph = 0;
		std::size_t GlyphCount = 0;
		int16 EndUnits = 0;
		EPadButton Button = EPadButton::Select;
		bool bDirections = false;
	};

	/**
	 * The buttons a typed line names in words: L1, L2, R1, R2, Start, Select or Directional as a whole word, followed by
	 * "button" or "buttons" (either case), or by "and" and another such name and those. The glyphs may read on past the
	 * line's end into the next line's (a MesLineEnd between them), where the text breaks a name from its word: "the L1 /
	 * button", "the L2 and / R2 buttons". Out keeps its memory.
	 */
	WELTALLCORE_API void FindMesButtonWords(std::span<const FMesPlacedGlyph> Glyphs, std::vector<FMesButtonWord>& Out);
}
