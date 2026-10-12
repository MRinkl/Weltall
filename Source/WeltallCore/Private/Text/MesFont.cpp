#include "Text/MesFont.h"

#include "Foundation/LittleEndian.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr std::size_t HeaderBytes = 14;
		constexpr int32 GlyphRows = 11;
		constexpr int32 GlyphColumns = 12;
		constexpr uint8 BoxLead = 0xFF;
		constexpr uint8 BoxCode = 0xFF;

		/** A glyph pixel: rows are big-endian with the leftmost pixel in bit 15. */
		bool IsSet(std::span<const uint8> Glyph, int32 X, int32 Y)
		{
			if (X < 0 || X >= GlyphColumns || Y < 0 || Y >= GlyphRows)
			{
				return false;
			}
			const uint16 Row = static_cast<uint16>(Glyph[static_cast<std::size_t>(Y) * 2] << 8 | Glyph[static_cast<std::size_t>(Y) * 2 + 1]);
			return (Row >> (15 - X) & 1) != 0;
		}
	}

	TResult<FMesFont> FMesFont::Parse(std::span<const uint8> Font, std::span<const uint8> BoxGlyph)
	{
		if (Font.size() < HeaderBytes || BoxGlyph.size() < GlyphBytes)
		{
			return MakeError("the message font is too short");
		}
		const uint16 GlyphsOffset = LittleEndian::ReadU16(Font, 2);
		if (GlyphsOffset > Font.size())
		{
			return MakeError("the message font's glyphs lie outside it");
		}
		FMesFont Result;
		Result.SecondPageLead = static_cast<uint8>(LittleEndian::ReadU16(Font, 4));
		Result.SecondPageOffset = LittleEndian::ReadU16(Font, 6);
		Result.NarrowCount = LittleEndian::ReadU16(Font, 8);
		Result.SecondPageNarrowCount = LittleEndian::ReadU16(Font, 10);
		Result.FirstCode = LittleEndian::ReadU16(Font, 12);
		Result.Glyphs.assign(Font.begin() + GlyphsOffset, Font.end());
		std::copy_n(BoxGlyph.begin(), GlyphBytes, Result.Box.begin());
		return Result;
	}

	int32 FMesFont::WidthUnits(FMesCode Code) const
	{
		// As the kernel compares them: a one-byte code before the first glyph counts as narrow too.
		bool bNarrow = false;
		if (Code.Lead == 0)
		{
			bNarrow = static_cast<int32>(Code.Code) - static_cast<int32>(FirstCode) < static_cast<int32>(NarrowCount);
		}
		else if (Code.Lead == SecondPageLead)
		{
			bNarrow = Code.Code < SecondPageNarrowCount;
		}
		return bNarrow ? 2 : 3;
	}

	FMesFont::FCell FMesFont::Cell(FMesCode Code) const
	{
		std::span<const uint8> Glyph;
		if (Code.Lead == BoxLead && Code.Code == BoxCode)
		{
			Glyph = Box;
		}
		else
		{
			// Codes before the first glyph, or past the font, have no glyph (the kernel would draw
			// whatever memory lies there; no text uses them).
			if (Code.Lead == 0 ? Code.Code < FirstCode : Code.Lead < SecondPageLead)
			{
				return {};
			}
			const std::size_t Offset = Code.Lead == 0
				? (Code.Code - FirstCode) * GlyphBytes
				: SecondPageOffset + static_cast<std::size_t>(Code.Lead - SecondPageLead) * 0x1600 + Code.Code * GlyphBytes;
			if (Offset + GlyphBytes > Glyphs.size())
			{
				return {};
			}
			Glyph = std::span<const uint8>(Glyphs).subspan(Offset, GlyphBytes);
		}
		FCell Result{};
		for (int32 Y = 0; Y < CellHeight; ++Y)
		{
			for (int32 X = 0; X < CellWidth; ++X)
			{
				const int32 GlyphX = X - 1;
				const int32 GlyphY = Y - 1;
				bool bOutline = false;
				for (int32 Dy = -1; Dy <= 1 && !bOutline; ++Dy)
				{
					for (int32 Dx = -1; Dx <= 1; ++Dx)
					{
						if ((Dx != 0 || Dy != 0) && IsSet(Glyph, GlyphX + Dx, GlyphY + Dy))
						{
							bOutline = true;
							break;
						}
					}
				}
				Result[static_cast<std::size_t>(Y * CellWidth + X)] = static_cast<uint8>((IsSet(Glyph, GlyphX, GlyphY) ? 1 : 0) | (bOutline ? 2 : 0));
			}
		}
		return Result;
	}
}
