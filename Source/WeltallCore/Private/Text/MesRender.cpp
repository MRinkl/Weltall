#include "Text/MesRender.h"

namespace Weltall
{
	namespace
	{
		/** The renderer types a whole line in one step: up to 100 glyphs. */
		constexpr uint8 LineGlyphs = 100;
		constexpr int32 UnitPixels = 4;
		constexpr int32 GlyphUnits = 3;
	}

	void DrawMesGlyph(const FMesFont* Font, FMesCode Code, std::span<uint16> Units, int32 Pitch, int32 XUnits, int32 Plane)
	{
		const uint16 Kept = Plane == 0 ? 0xCCCC : 0x3333;
		const FMesFont::FCell Cell = Font != nullptr ? Font->Cell(Code) : FMesFont::FCell{};
		for (int32 Row = 0; Row < FMesFont::CellHeight; ++Row)
		{
			for (int32 Unit = 0; Unit < GlyphUnits; ++Unit)
			{
				Units[static_cast<std::size_t>(Row * Pitch + XUnits + Unit)] &= Kept;
			}
			for (int32 Pixel = 0; Pixel < FMesFont::CellWidth; ++Pixel)
			{
				const uint32 Value = Cell[static_cast<std::size_t>(Row * FMesFont::CellWidth + Pixel)];
				uint16& Unit = Units[static_cast<std::size_t>(Row * Pitch + XUnits + Pixel / UnitPixels)];
				Unit = static_cast<uint16>(Unit | Value << (Pixel % UnitPixels * 4 + Plane * 2));
			}
		}
	}

	int32 RenderMesString(FMesContext& Context, FMesWindow& Typing, std::span<const uint8> String, FVramImage& Image, int32 Plane)
	{
		const FMesWindow::FLine& Line = Typing.TypeLine(Context, String, LineGlyphs);
		for (const FMesPlacedGlyph& Glyph : Line.Glyphs)
		{
			DrawMesGlyph(Context.Font, Glyph.Code, Image.Units, Image.Rect.Width, Glyph.XUnits, Plane);
		}
		return Line.WidthUnits * UnitPixels;
	}
}
