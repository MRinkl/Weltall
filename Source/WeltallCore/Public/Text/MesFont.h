#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	/** A glyph as text stores it: one byte (lead 0), or a lead byte (FEh, FFh) and a second byte. */
	struct FMesCode
	{
		uint8 Lead = 0;
		uint8 Code = 0;

		bool operator==(const FMesCode&) const = default;
	};

	/**
	 * The game's message font: glyphs of 11 rows of 12 pixels, and the box glyph the kernel carries
	 * for codes the font lacks (FF FF). The kernel draws a glyph into a 12 x 13 cell of 2-bit values:
	 * 1 for its body, moved one pixel right and down, 2 for an outline around the body (3 where a
	 * body pixel also borders another), clipped to the cell.
	 */
	class FMesFont
	{
	public:
		static constexpr int32 CellWidth = 12;
		static constexpr int32 CellHeight = 13;
		static constexpr std::size_t GlyphBytes = 22;
		using FCell = std::array<uint8, CellWidth * CellHeight>;

		/** The font as its file unpacks (directory 1, file 6), and the executable's box glyph. */
		WELTALLCORE_API static TResult<FMesFont> Parse(std::span<const uint8> Font, std::span<const uint8> BoxGlyph);

		/** The lead byte from which codes take a second byte. */
		uint8 LeadByte() const { return SecondPageLead; }

		/** How far the text advances after a glyph, in units of 4 pixels: 2 for the narrow ones, 3 for the rest. */
		WELTALLCORE_API int32 WidthUnits(FMesCode Code) const;

		/** The glyph's cell; empty for a code the font does not hold. */
		WELTALLCORE_API FCell Cell(FMesCode Code) const;

	private:
		std::vector<uint8> Glyphs;
		std::array<uint8, GlyphBytes> Box{};
		uint8 SecondPageLead = 0;
		uint32 SecondPageOffset = 0;
		uint32 NarrowCount = 0;
		uint32 SecondPageNarrowCount = 0;
		uint32 FirstCode = 0;
	};
}
