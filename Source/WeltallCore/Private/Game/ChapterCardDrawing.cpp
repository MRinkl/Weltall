#include "Game/ChapterCardDrawing.h"

#include "Game/ChapterCard.h"
#include "Game/ChapterTitles.h"
#include "Text/MesFont.h"
#include "Text/MesGlyphs.h"

#include <algorithm>
#include <vector>

namespace Weltall
{
	namespace
	{
		/**
		 * The line's two pictures in 15-bit pages at VRAM's corner, as wide as the screen: the letters with their
		 * outlines, and the letters alone under them. A sprite reads no more than a page's width across, so a line
		 * wider than a page is drawn as two sprites, the second from the page beside the first.
		 */
		constexpr int32 PictureWidth = ChapterCardScreen.Width;
		constexpr int32 PageWidth = 256;
		constexpr int32 ShadeV = 0;
		constexpr int32 LettersV = 16;
		/** White, with the bit that lets a texel blend. */
		constexpr uint16 Ink = 0xFFFF;
		constexpr int32 PixelsPerWidthUnit = 4;
		/** The band: its rows on the screen, the rows over which each edge fades, and how dark its middle gets. */
		constexpr int16 BandTop = 92;
		constexpr int16 BandHeight = 40;
		constexpr int16 BandEdge = 12;
		constexpr int32 BandDarkness = 88;
		/** Far past any display's side, on the screen's scale. */
		constexpr int16 Beyond = 8192;
		constexpr FScreenRect WholeWidth{-Beyond, 0, 320 + 2 * Beyond, 224};

		FDrawColor Grey(int32 Level)
		{
			const uint8 Value = static_cast<uint8>(std::clamp(Level, 0, 255));
			return {Value, Value, Value};
		}

		/** A band's rows across the whole display, darkening by Top at its top and Bottom at its foot. */
		FDrawPrimitive Band(int16 Y, int16 Height, int32 Top, int32 Bottom)
		{
			FDrawPrimitive Rows = MakeGouraudRect({WholeWidth.X, Y, WholeWidth.Width, Height}, {Grey(Top), Grey(Top), Grey(Bottom), Grey(Bottom)});
			Rows.Blend = EDrawBlend::SemiTransparent;
			Rows.UntexturedBlend = EBlendMode::Subtract;
			Rows.Clip = WholeWidth;
			return Rows;
		}

		/** A picture's Width columns from row V as sprites at X and Y, a page's width each. */
		void AddTextSprites(FDrawList& Out, int32 X, int32 Y, int32 Width, int32 V, EBlendMode Blend, int32 Level)
		{
			for (int32 Left = 0; Left < Width; Left += PageWidth)
			{
				FDrawTexture Texture;
				Texture.Page.X = Left;
				Texture.Page.Depth = ETextureDepth::Direct15;
				Texture.Page.Blend = Blend;
				FDrawPrimitive Sprite = MakeSprite(X + Left, Y, std::min(PageWidth, Width - Left), FMesFont::CellHeight, Texture, 0, static_cast<uint8>(V), Grey(Level));
				Sprite.Blend = EDrawBlend::SemiTransparent;
				Sprite.Clip = WholeWidth;
				Out.Primitives.push_back(Sprite);
			}
		}
	}

	std::string FChapterCardDrawing::Text(const FChapter& Chapter)
	{
		return "Chapter " + std::to_string(Chapter.Number) + ": " + Chapter.Title;
	}

	std::string FChapterCardDrawing::Text(const FChapter& Chapter, int32 Part, std::string_view Subtitle)
	{
		return "Chapter " + std::to_string(Chapter.Number) + static_cast<char>('a' + Part - 1) + ": " + std::string(Subtitle);
	}

	int32 FChapterCardDrawing::DrawText(const std::string& Line, const FMesFont& Font)
	{
		FVramImage Shade{{0, ShadeV, PictureWidth, FMesFont::CellHeight}, std::vector<uint16>(PictureWidth * FMesFont::CellHeight, 0)};
		FVramImage Letters{{0, LettersV, PictureWidth, FMesFont::CellHeight}, std::vector<uint16>(PictureWidth * FMesFont::CellHeight, 0)};
		int32 Left = 0;
		for (const char Character : Line)
		{
			const FMesCode Code = MesCodeOfCharacter(Character).value_or(*MesCodeOfCharacter(' '));
			const int32 Advance = Font.WidthUnits(Code) * PixelsPerWidthUnit;
			if (Left + FMesFont::CellWidth > PictureWidth)
			{
				break;
			}
			const FMesFont::FCell Cell = Font.Cell(Code);
			for (int32 Row = 0; Row < FMesFont::CellHeight; ++Row)
			{
				for (int32 Column = 0; Column < FMesFont::CellWidth; ++Column)
				{
					// A cell's value: 1 the glyph's body, 2 its outline, 3 a body pixel beside another.
					const uint8 Value = Cell[static_cast<std::size_t>(Row * FMesFont::CellWidth + Column)];
					const std::size_t At = static_cast<std::size_t>(Row * PictureWidth + Left + Column);
					if (Value != 0)
					{
						Shade.Units[At] = Ink;
					}
					if ((Value & 1) != 0)
					{
						Letters.Units[At] = Ink;
					}
				}
			}
			Left += Advance;
		}
		PictureVram.Upload(Shade);
		PictureVram.Upload(Letters);
		// The last glyph's cell may reach past its advance.
		return std::min(Left + FMesFont::CellWidth, PictureWidth);
	}

	void FChapterCardDrawing::Build(std::string_view Line, int32 Level, const FMesFont& Font, FDrawList& Out)
	{
		Out.Primitives.clear();
		if (Level <= 0)
		{
			return;
		}
		if (Drawn != Line)
		{
			Drawn = std::string(Line);
			TextWidth = DrawText(Drawn, Font);
		}
		const int32 Dark = BandDarkness * Level / FChapterCard::FullLevel;
		Out.Primitives.push_back(Band(BandTop, BandEdge, 0, Dark));
		Out.Primitives.push_back(Band(BandTop + BandEdge, BandHeight - 2 * BandEdge, Dark, Dark));
		Out.Primitives.push_back(Band(BandTop + BandHeight - BandEdge, BandEdge, Dark, 0));
		// A texel is doubled at 255: the outlines take everything away, the letters come out white.
		const int32 Strength = 255 * Level / FChapterCard::FullLevel;
		const int32 X = (ChapterCardScreen.Width - (TextWidth - FMesFont::CellWidth)) / 2;
		const int32 Y = BandTop + (BandHeight - FMesFont::CellHeight) / 2;
		AddTextSprites(Out, X, Y, TextWidth, ShadeV, EBlendMode::Subtract, Strength);
		AddTextSprites(Out, X, Y, TextWidth, LettersV, EBlendMode::Add, Strength);
	}
}
