#include "Game/BattleSpeedMark.h"

#include "Text/MesFont.h"
#include "Text/MesGlyphs.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Weltall
{
	namespace
	{
		/** The game screen's lines, and how many the mark keeps from the picture's edges. */
		constexpr double ScreenLines = 224.0;
		constexpr double MarginLines = 5.0;

		/** An arrowhead's box, how far the second starts after the first, and the dark edge's width around the white. */
		constexpr float ArrowWidth = 8.5f;
		constexpr float ArrowHeight = 10.0f;
		constexpr float ArrowStep = 7.5f;
		constexpr float ArrowEdge = 1.0f;
		constexpr float ArrowsWidth = ArrowStep + ArrowWidth;
		/** The text's row under the arrows. */
		constexpr int32 TextTop = 11;

		constexpr FDrawColor Dark{16, 16, 24};
		constexpr FDrawColor Light{248, 248, 248};
		/** The text's picture in a 15-bit page at VRAM's corner: white letters, the arrows' dark around them, nothing elsewhere. */
		constexpr int32 PictureWidth = 32;
		constexpr uint16 Ink = 0x7FFF;
		constexpr uint16 Outline = static_cast<uint16>(0x8000 | (Dark.B >> 3) << 10 | (Dark.G >> 3) << 5 | Dark.R >> 3);

		/** An arrowhead pointing right from Left, drawn Inset inside its box's outline. */
		FDrawPrimitive Arrow(float Left, float Inset, FDrawColor Color)
		{
			// The slanted sides move in along their normals: further along each axis than Inset itself.
			const float Half = ArrowHeight / 2.0f;
			const float Slant = std::sqrt(ArrowWidth * ArrowWidth + Half * Half);
			const float Top = Half * Inset / ArrowWidth + Inset * Slant / ArrowWidth;
			FDrawPrimitive Triangle;
			Triangle.CornerCount = 3;
			Triangle.Corners[0] = {Left + Inset, Top, Color};
			Triangle.Corners[1] = {Left + Inset, ArrowHeight - Top, Color};
			Triangle.Corners[2] = {Left + ArrowWidth - Inset * Slant / Half, Half, Color};
			return Triangle;
		}
	}

	void FBattleSpeedMarkDrawing::DrawText(std::string_view Text, const FMesFont& Font)
	{
		FVramImage Picture{{0, 0, PictureWidth, FMesFont::CellHeight}, std::vector<uint16>(PictureWidth * FMesFont::CellHeight, 0)};
		TextWidth = 0;
		for (const char Character : Text)
		{
			const FMesFont::FCell Cell = Font.Cell(MesCodeOfCharacter(Character).value_or(*MesCodeOfCharacter(' ')));
			// A cell's value: 1 the glyph's body, 2 its outline, 3 a body pixel beside another.
			const auto Value = [&Cell](int32 Row, int32 Column) { return Cell[static_cast<std::size_t>(Row * FMesFont::CellWidth + Column)]; };
			// The glyphs stand side by side, outline against outline: the font's own advance would part two of them widely.
			int32 First = FMesFont::CellWidth;
			int32 Last = -1;
			for (int32 Row = 0; Row < FMesFont::CellHeight; ++Row)
			{
				for (int32 Column = 0; Column < FMesFont::CellWidth; ++Column)
				{
					if (Value(Row, Column) != 0)
					{
						First = std::min(First, Column);
						Last = std::max(Last, Column);
					}
				}
			}
			if (Last < First || TextWidth + Last - First + 1 > PictureWidth)
			{
				continue;
			}
			for (int32 Row = 0; Row < FMesFont::CellHeight; ++Row)
			{
				for (int32 Column = First; Column <= Last; ++Column)
				{
					if (Value(Row, Column) != 0)
					{
						Picture.Units[static_cast<std::size_t>(Row * PictureWidth + TextWidth + Column - First)] = (Value(Row, Column) & 1) != 0 ? Ink : Outline;
					}
				}
			}
			TextWidth += Last - First + 1;
		}
		PictureVram.Upload(Picture);
	}

	void FBattleSpeedMarkDrawing::Build(EBattleSpeed Speed, const FMesFont& Font, FDrawList& Out)
	{
		Out.Primitives.clear();
		if (Speed == EBattleSpeed::Normal)
		{
			return;
		}
		if (Drawn != Speed)
		{
			DrawText(BattleSpeedName(Speed), Font);
			Drawn = Speed;
		}
		const float ArrowsLeft = static_cast<float>(BattleSpeedMarkScreen.Width) - ArrowsWidth;
		for (const float Left : {ArrowsLeft, ArrowsLeft + ArrowStep})
		{
			Out.Primitives.push_back(Arrow(Left, 0.0f, Dark));
		}
		for (const float Left : {ArrowsLeft, ArrowsLeft + ArrowStep})
		{
			Out.Primitives.push_back(Arrow(Left, ArrowEdge, Light));
		}
		FDrawTexture Texture;
		Texture.Page.Depth = ETextureDepth::Direct15;
		Out.Primitives.push_back(MakeSprite(BattleSpeedMarkScreen.Width - TextWidth, TextTop, TextWidth, FMesFont::CellHeight, Texture, 0, 0));
	}

	FDisplayRect PlaceBattleSpeedMark(int32 Width, int32 Height)
	{
		const double PixelsPerLine = Height / ScreenLines;
		const int32 Margin = static_cast<int32>(std::lround(MarginLines * PixelsPerLine));
		FDisplayRect Place;
		Place.Width = static_cast<int32>(std::lround(BattleSpeedMarkScreen.Width * PixelsPerLine));
		Place.Height = static_cast<int32>(std::lround(BattleSpeedMarkScreen.Height * PixelsPerLine));
		Place.X = Width - Margin - Place.Width;
		Place.Y = Margin;
		return Place;
	}
}
