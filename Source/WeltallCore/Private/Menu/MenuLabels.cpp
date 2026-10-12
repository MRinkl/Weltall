#include "Menu/MenuModule.h"
#include "Menu/MenuResources.h"
#include "Text/MesGlyphs.h"
#include "Text/MesRender.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** The labels' strips: pairs of 28-unit, 13-row strips from (320, 0), two columns across, the base's rows down. */
		constexpr int16 StripX = 0x140;
		constexpr int16 StripColumn = 0x20;
		constexpr int16 StripWidth = 0x1C;
		constexpr int16 StripRows = 13;
		/** The labels' 4-bit page, and the message font's CLUTs the menu loads for their two planes. */
		constexpr int32 LabelPageX = 0x140;
		constexpr int32 TextClutX = 0;
		constexpr int32 TextClutY = 0x1D1;
		constexpr int32 OddClutX = 16;
		/** A prompt's lines: typed two to a strip from row 78, placed 16 apart. */
		constexpr int32 PromptStripY = 0x4E;
		constexpr int32 PromptStep = 0x10;
		/** The lists' strips' page: names and numbers typed in pairs from (384, 128). */
		constexpr int32 StripPageX = 0x180;
		constexpr int32 StripPageY = 0x80;
	}

	void FMenuModule::ClearFlags(int32 First, int32 Count)
	{
		for (int32 Flag = First; Flag < First + Count; ++Flag)
		{
			Flags[static_cast<std::size_t>(Flag)] = 0;
		}
	}

	int32 FMenuModule::RenderString(FMesWindow& Typing, FVramImage& Image, std::span<const uint8> Text, int32 Plane)
	{
		return RenderMesString(Services.Text, Typing, Text, Image, Plane);
	}

	int32 FMenuModule::RenderPlain(FMesWindow& Typing, FVramImage& Image, std::string_view Text, int32 Plane)
	{
		GlyphScratch.clear();
		for (const char Character : Text)
		{
			const FMesCode Glyph = MesCodeOfCharacter(Character).value_or(*MesCodeOfCharacter(' '));
			if (Glyph.Lead != 0)
			{
				GlyphScratch.push_back(Glyph.Lead);
			}
			GlyphScratch.push_back(Glyph.Code);
		}
		GlyphScratch.push_back(0);
		return RenderString(Typing, Image, GlyphScratch, Plane);
	}

	void FMenuModule::RenderLabels(std::span<FMenuLabel> Labels, std::span<const uint8> Messages, int32 Base)
	{
		for (std::size_t Index = 0; Index + 1 < Messages.size() && Index + 1 < Labels.size(); Index += 2)
		{
			FMenuLabel& First = Labels[Index];
			FMenuLabel& Second = Labels[Index + 1];
			First.Width = static_cast<uint8>(RenderString(LabelTyping, LabelImage, Resources.Text().String(Messages[Index]), 0));
			Second.Width = static_cast<uint8>(RenderString(LabelTyping, LabelImage, Resources.Text().String(Messages[Index + 1]), 1));
			const int32 Pair = static_cast<int32>(Index) / 2;
			First.Strip = {static_cast<int16>(StripX + (Pair & 1) * StripColumn), static_cast<int16>((static_cast<int32>(Index) + Base) / 4 * StripRows), StripWidth,
				StripRows};
			Second.Strip = First.Strip;
			SetLabelTexture(First, static_cast<int32>(Index), Base);
			SetLabelTexture(Second, static_cast<int32>(Index) + 1, Base);
			LabelImage.Rect = {First.Strip.X, First.Strip.Y, StripWidth, StripRows};
			Upload(LabelImage);
		}
	}

	void FMenuModule::SetLabelTexture(FMenuLabel& Label, int32 Index, int32 Base)
	{
		FMenuPrimitive& Quad = Label.Quad;
		Quad.Code = MenuGpu::TexturedQuad;
		Quad.Corners[0].Color = {0x80, 0x80, 0x80};
		Label.Plane = static_cast<uint8>(Index & 1);
		Quad.Page = MenuGpu::TexturePage(0, 0, LabelPageX, 0);
		const uint8 U = static_cast<uint8>(Index / 2 * -0x80);
		const uint8 V = static_cast<uint8>((Index + Base) / 4 * StripRows);
		const uint8 Right = static_cast<uint8>(U + Label.Width);
		const uint8 Bottom = static_cast<uint8>(V + StripRows);
		Quad.Corners[0].U = U;
		Quad.Corners[0].V = V;
		Quad.Corners[1].U = Right;
		Quad.Corners[1].V = V;
		Quad.Corners[2].U = U;
		Quad.Corners[2].V = Bottom;
		Quad.Corners[3].U = Right;
		Quad.Corners[3].V = Bottom;
		Quad.Clut = MenuGpu::Clut(Label.Plane == 0 ? TextClutX : OddClutX, TextClutY);
		MirrorLabelTexture(Label);
	}

	void FMenuModule::RecordLabel(FMenuLabel& Label)
	{
		if (Label.Buffer == Buffer)
		{
			return;
		}
		const std::array<FGteVector, 4> Points = Label.Quad.Points;
		std::swap(Label.Quad, Label.Other);
		Label.Quad.Points = Points;
		Label.Buffer = Buffer;
	}

	void FMenuModule::MirrorLabelTexture(FMenuLabel& Label)
	{
		FMenuPrimitive& Other = Label.Other;
		Other.Code = Label.Quad.Code;
		Other.Page = Label.Quad.Page;
		Other.Clut = Label.Quad.Clut;
		for (std::size_t Corner = 0; Corner < 4; ++Corner)
		{
			Other.Corners[Corner].U = Label.Quad.Corners[Corner].U;
			Other.Corners[Corner].V = Label.Quad.Corners[Corner].V;
		}
		Other.Corners[0].Color = Label.Quad.Corners[0].Color;
	}

	void FMenuModule::SetStripLabelTexture(FMenuLabel& Label, int32 Index, int32 Base, uint8 Mode)
	{
		FMenuPrimitive& Quad = Label.Quad;
		Quad.Code = MenuGpu::TexturedQuad;
		Quad.Corners[0].Color = {0x80, 0x80, 0x80};
		uint16 Blend = 0;
		if ((Mode & 0x80) == 0)
		{
			// Dimmed: half the brightness, blended.
			Blend = MenuGpu::PageBlendAdd;
			Quad.Code = static_cast<uint8>(Quad.Code | MenuGpu::SemiTransparent);
			Quad.Corners[0].Color = {0x20, 0x20, 0x20};
		}
		Label.Plane = static_cast<uint8>((Mode & 0x7F) - 1);
		Quad.Page = static_cast<uint16>(MenuGpu::TexturePage(0, 0, StripPageX, StripPageY) | Blend);
		const uint8 U = static_cast<uint8>((Index & 1) * 0x60);
		const uint8 V = static_cast<uint8>(Index / 2 * StripRows + Base);
		Quad.Corners[0].U = U;
		Quad.Corners[0].V = V;
		Quad.Corners[1].U = static_cast<uint8>(U + Label.Width);
		Quad.Corners[1].V = V;
		Quad.Corners[2].U = U;
		Quad.Corners[2].V = static_cast<uint8>(V + StripRows);
		Quad.Corners[3].U = static_cast<uint8>(U + Label.Width);
		Quad.Corners[3].V = static_cast<uint8>(V + StripRows);
		Quad.Clut = MenuGpu::Clut(Label.Plane == 0 ? 0 : OddClutX, TextClutY);
		MirrorLabelTexture(Label);
	}

	void FMenuModule::GlyphText(std::span<const uint16> Glyphs, std::vector<uint8>& Text) const
	{
		Text.clear();
		for (const uint16 Glyph : Glyphs)
		{
			const std::span<const uint8> Entry = Services.Text.Tables->GlyphMapEntry(Glyph);
			if (Entry.size() < 2)
			{
				continue;
			}
			if (Entry[0] != 0)
			{
				Text.push_back(Entry[0]);
			}
			Text.push_back(Entry[1]);
		}
		Text.push_back(0);
	}

	void FMenuModule::RenderPromptLines(std::span<FMenuLabel> Lines, uint8 FirstText, int32 X, int32 Y, std::string_view SecondLine)
	{
		for (std::size_t Line = 0; Line < Lines.size(); ++Line)
		{
			const int32 Index = static_cast<int32>(Line);
			const int32 V = Index / 2 * StripRows + PromptStripY;
			if (Index % 2 == 0)
			{
				if (Index > 0)
				{
					Upload(PromptImage);
				}
				std::fill(PromptImage.Units.begin(), PromptImage.Units.end(), uint16{0});
				PromptImage.Rect.X = StripX;
				PromptImage.Rect.Y = static_cast<int16>(V);
			}
			FMenuLabel& Label = Lines[Line];
			Label.Width = static_cast<uint8>(Line == 1 && !SecondLine.empty()
					? RenderPlain(PromptTyping, PromptImage, SecondLine, Index % 2)
					: RenderString(PromptTyping, PromptImage, Resources.Text().String(static_cast<uint8>(FirstText + Line)), Index % 2));
			SetLabelTexture(Label, Index, 0);
			SetLayerQuad(Label.Quad.Points, X, (Index * PromptStep + Y) & 0xFFF0, Label.Width, StripRows);
			RecordLabel(Label);
			SetTexelRect(Label.Quad, 0, V, Label.Width, StripRows);
		}
		Upload(PromptImage);
	}
}
