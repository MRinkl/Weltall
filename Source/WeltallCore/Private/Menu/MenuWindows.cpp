#include "Menu/MenuModule.h"

namespace Weltall
{
	namespace
	{
		/** The sheet's frames whose textures a window's edges take: top, bottom, left, right. */
		constexpr std::array<int32, 4> EdgeFrames = {0xFE, 0x103, 0x100, 0x101};
		/** The frame's corners: top left, top right, bottom left, bottom right. */
		constexpr std::array<int32, 4> CornerFrames = {0xFD, 0xFF, 0x102, 0x104};
		/** Each edge's texels in its page: (u, v) of its top left and bottom right. */
		constexpr std::array<std::array<uint8, 4>, 4> EdgeTexels = {{{0, 0x84, 7, 0x94}, {8, 0x84, 15, 0x94}, {0x10, 0x84, 0x20, 0x8B}, {0x10, 0x8C, 0x20, 0x93}}};
		constexpr uint8 FillGrey = 0x68;
		constexpr uint8 Unshaded = 0x80;
		constexpr int32 Grows = 32;
		constexpr int32 CornerHalf = 8;
		constexpr int32 FullScale = 0x1000;
		constexpr int32 PiecesEntry = 4;
		/** The screen's centre, which the layer's points are measured from. */
		constexpr int32 CentreX = 160;
		constexpr int32 CentreY = 112;
		constexpr int32 FirstGrowingFlag = MenuFlag::Windows + 7;
		/** A list's scroll bar: its ends (the bottom one mirrored), the track between, 8 pixels wide. */
		constexpr int32 ScrollBarEndFrame = 0x105;
		constexpr int32 ScrollBarTrackFrame = 0x106;
		constexpr int32 ScrollBarEnd = 8;

		void SetQuad(FMenuPrimitive& Quad, int32 X, int32 Y, int32 Width, int32 Height)
		{
			SetLayerQuad(Quad.Points, X, Y, Width, Height);
		}

		/** A quad made semi-transparent and modulated by a neutral grey, as the menu dims its frames' pieces. */
		void SetSemiTransparent(FMenuPrimitive& Quad)
		{
			Quad.Code = static_cast<uint8>((Quad.Code | MenuGpu::SemiTransparent) & ~MenuGpu::RawTexture);
			Quad.Corners[0].Color = {Unshaded, Unshaded, Unshaded};
		}
	}

	void SetLayerQuad(std::array<FGteVector, 4>& Points, int32 X, int32 Y, int32 Width, int32 Height)
	{
		// In 16 bits, as the menu keeps them.
		const int16 Left = static_cast<int16>(X - CentreX);
		const int16 Right = static_cast<int16>(X + Width - CentreX);
		const int16 Top = static_cast<int16>(Y - CentreY);
		const int16 Bottom = static_cast<int16>(Y + Height - CentreY);
		Points = {FGteVector{Left, Top, 0}, FGteVector{Right, Top, 0}, FGteVector{Left, Bottom, 0}, FGteVector{Right, Bottom, 0}};
	}

	void SetTexelRect(FMenuPrimitive& Quad, int32 U, int32 V, int32 Width, int32 Height)
	{
		const uint8 Left = static_cast<uint8>(U);
		const uint8 Right = static_cast<uint8>(U + Width);
		const uint8 Top = static_cast<uint8>(V);
		const uint8 Bottom = static_cast<uint8>(V + Height);
		Quad.Corners[0].U = Left;
		Quad.Corners[0].V = Top;
		Quad.Corners[1].U = Right;
		Quad.Corners[1].V = Top;
		Quad.Corners[2].U = Left;
		Quad.Corners[2].V = Bottom;
		Quad.Corners[3].U = Right;
		Quad.Corners[3].V = Bottom;
	}

	void SetLayerSprites(std::vector<FMenuPrimitive>& Parts, std::size_t First)
	{
		for (std::size_t Part = First; Part < Parts.size(); ++Part)
		{
			const std::array<FMenuCorner, 4>& At = Parts[Part].Corners;
			SetLayerQuad(Parts[Part].Points, At[0].X, At[0].Y, At[1].X - At[0].X, At[2].Y - At[0].Y);
		}
	}

	void FMenuModule::AddLayered(std::vector<FMenuPrimitive>& Parts)
	{
		for (FMenuPrimitive& Part : Parts)
		{
			AddProjected(Part, true, PiecesEntry);
		}
	}

	void FMenuModule::OpenWindow(int32 Index, int16 X, int16 Y, uint16 Width, uint16 Height, bool bGrows, bool bOnLayer, int32 Entry, bool bScrollBar)
	{
		// The first two windows are made with the Normal Menu; the others each time they open.
		if (Index > 1)
		{
			InitWindowPrims(Index);
		}
		if (!bGrows)
		{
			BuildWindow(Index, X, Y, Width, Height, bOnLayer, Entry, bScrollBar);
			return;
		}
		FMenuWindowGrowth& Growth = Growths[static_cast<std::size_t>(Index)];
		Growth = {X, Y, Width, Height, 0, 0, Entry, false, bOnLayer, bScrollBar};
		Flags[static_cast<std::size_t>(FirstGrowingFlag + Index)] = 1;
	}

	void FMenuModule::InitWindowPrims(int32 Index)
	{
		FMenuWindow& Window = Windows[static_cast<std::size_t>(Index)];
		Window = {};
		Flags[static_cast<std::size_t>(MenuFlag::Windows + Index)] = 0;
		Flags[static_cast<std::size_t>(FirstGrowingFlag + Index)] = 0;
		const FMenuSpriteSheet& Sheet = Resources.Sheet();
		Window.Fill.Code = MenuGpu::GouraudQuad | MenuGpu::SemiTransparent;
		for (FMenuCorner& At : Window.Fill.Corners)
		{
			At.Color = {FillGrey, FillGrey, FillGrey};
		}
		const FMenuSpriteTexture TopTexture = Sheet.Texture(EdgeFrames[0]);
		Window.FillMode = FMenuPrimitive::MakeDrawMode(MenuGpu::TexturePage(0, 0, TopTexture.VramX, TopTexture.VramY));
		for (std::size_t Edge = 0; Edge < Window.Edges.size(); ++Edge)
		{
			const FMenuSpriteTexture Texture = Sheet.Texture(EdgeFrames[Edge / 2]);
			FMenuPrimitive& Quad = Window.Edges[Edge];
			Quad.Code = MenuGpu::TexturedQuad | MenuGpu::RawTexture;
			Quad.Page = MenuGpu::TexturePage(Texture.Depth, 0, Texture.VramX, Texture.VramY);
			Quad.Clut = MenuGpu::Clut(Texture.ClutX, Texture.ClutY);
		}
	}

	void FMenuModule::BuildWindow(int32 Index, int16 X, int16 Y, uint16 Width, uint16 Height, bool bOnLayer, int32 Entry, bool bScrollBar)
	{
		FMenuWindow& Window = Windows[static_cast<std::size_t>(Index)];
		Flags[static_cast<std::size_t>(MenuFlag::Windows + Index)] = 0;
		SetQuad(Window.Fill, X, Y, Width, Height);
		// The corners: textured from the sheet, placed centred on the window's corners, the top ones turned upside down.
		Scratch.clear();
		for (const int32 Frame : CornerFrames)
		{
			Resources.Sheet().Draw(Frame, 0, 0, FullScale, Scratch);
		}
		for (std::size_t Piece = 0; Piece < Window.Corners.size() && Piece < Scratch.size(); ++Piece)
		{
			Window.Corners[Piece] = Scratch[Piece];
		}
		SetQuad(Window.Corners[0], X - CornerHalf, Y + CornerHalf, 2 * CornerHalf, -2 * CornerHalf);
		SetQuad(Window.Corners[1], X + Width + CornerHalf, Y + CornerHalf, -2 * CornerHalf, -2 * CornerHalf);
		SetQuad(Window.Corners[2], X - CornerHalf, Y + Height - CornerHalf, 2 * CornerHalf, 2 * CornerHalf);
		SetQuad(Window.Corners[3], X + Width + CornerHalf, Y + Height - CornerHalf, -2 * CornerHalf, 2 * CornerHalf);
		for (FMenuPrimitive& Quad : Window.Corners)
		{
			SetSemiTransparent(Quad);
		}
		// The edges: two halves each, stretched from eight texels, the halves' lengths rounded toward zero.
		const int32 HalfWidth = (static_cast<int32>(Width) - 2 * CornerHalf) / 2;
		const int32 HalfHeight = (static_cast<int32>(Height) - 2 * CornerHalf) / 2;
		for (std::size_t Edge = 0; Edge < Window.Edges.size(); ++Edge)
		{
			FMenuPrimitive& Quad = Window.Edges[Edge];
			const std::array<uint8, 4>& Texels = EdgeTexels[Edge / 2];
			Quad.Corners[0].U = Texels[0];
			Quad.Corners[0].V = Texels[1];
			Quad.Corners[1].U = Texels[2];
			Quad.Corners[1].V = Texels[1];
			Quad.Corners[2].U = Texels[0];
			Quad.Corners[2].V = Texels[3];
			Quad.Corners[3].U = Texels[2];
			Quad.Corners[3].V = Texels[3];
			const int32 Half = static_cast<int32>(Edge % 2);
			switch (Edge / 2)
			{
			case 0:
				SetQuad(Quad, X + Half * HalfWidth + CornerHalf, Y - CornerHalf, HalfWidth, 2 * CornerHalf);
				break;
			case 1:
				SetQuad(Quad, X + Half * HalfWidth + CornerHalf, Y + Height - CornerHalf, HalfWidth, 2 * CornerHalf);
				break;
			case 2:
				SetQuad(Quad, X - CornerHalf, Y + Half * HalfHeight + CornerHalf, 2 * CornerHalf, HalfHeight);
				break;
			default:
				SetQuad(Quad, X + Width - CornerHalf, Y + Half * HalfHeight + CornerHalf, 2 * CornerHalf, HalfHeight);
				break;
			}
			SetSemiTransparent(Quad);
		}
		if (bScrollBar)
		{
			// Drawn as the sheet's frames are: raw and opaque, the bottom one mirrored.
			Scratch.clear();
			Resources.Sheet().Draw(ScrollBarEndFrame, X, Y, FullScale, Scratch);
			Resources.Sheet().DrawFlipped(ScrollBarEndFrame, X, Y + Height - ScrollBarEnd, FullScale, false, true, Scratch);
			Resources.Sheet().Draw(ScrollBarTrackFrame, X, Y + ScrollBarEnd, FullScale, Scratch);
			for (std::size_t Piece = 0; Piece < Window.ScrollBar.size() && Piece < Scratch.size(); ++Piece)
			{
				Window.ScrollBar[Piece] = Scratch[Piece];
			}
			SetQuad(Window.ScrollBar[0], X, Y, ScrollBarEnd, ScrollBarEnd);
			SetQuad(Window.ScrollBar[1], X, Y + Height, ScrollBarEnd, -ScrollBarEnd);
			SetQuad(Window.ScrollBar[2], X, Y + ScrollBarEnd, ScrollBarEnd, Height - ScrollBarEnd);
		}
		Window.bScrollBar = bScrollBar;
		Window.bOnLayer = bOnLayer;
		Window.Entry = Entry;
		Flags[static_cast<std::size_t>(MenuFlag::Windows + Index)] = 1;
	}

	void FMenuModule::GrowWindows()
	{
		for (int32 Index = 0; Index < static_cast<int32>(Growths.size()); ++Index)
		{
			FMenuWindowGrowth& Growth = Growths[static_cast<std::size_t>(Index)];
			if (Flags[static_cast<std::size_t>(FirstGrowingFlag + Index)] == 0 || Growth.bGrown)
			{
				continue;
			}
			int32 Reached = 0;
			if (Growth.GrownWidth + Grows < Growth.Width)
			{
				Growth.GrownWidth = static_cast<uint16>(Growth.GrownWidth + Grows);
			}
			else
			{
				Growth.GrownWidth = Growth.Width;
				++Reached;
			}
			if (Growth.GrownHeight + Grows < Growth.Height)
			{
				Growth.GrownHeight = static_cast<uint16>(Growth.GrownHeight + Grows);
			}
			else
			{
				Growth.GrownHeight = Growth.Height;
				++Reached;
			}
			Growth.bGrown = Reached == 2;
			BuildWindow(Index, static_cast<int16>(Growth.X + Growth.Width / 2 - Growth.GrownWidth / 2),
				static_cast<int16>(Growth.Y + Growth.Height / 2 - Growth.GrownHeight / 2), Growth.GrownWidth, Growth.GrownHeight, Growth.bOnLayer,
				Growth.Entry, Growth.bScrollBar);
		}
	}

	void FMenuModule::AddProjected(FMenuPrimitive& Quad, bool bOnLayer, int32 Entry)
	{
		const FGte& Gte = bOnLayer ? LayerGte : FlatGte;
		Quad.Space = bOnLayer ? EMenuSpace::Layer : EMenuSpace::Flat;
		for (std::size_t Point = 0; Point < 4; ++Point)
		{
			const FGteProjection Projected = Gte.Rtps(Quad.Points[Point]);
			Quad.Corners[Point].X = Projected.ScreenX;
			Quad.Corners[Point].Y = Projected.ScreenY;
		}
		Table.Add(Entry, Quad);
	}

	void FMenuModule::AddWindows()
	{
		for (int32 Index = 0; Index < static_cast<int32>(Windows.size()); ++Index)
		{
			if (Flags[static_cast<std::size_t>(MenuFlag::Windows + Index)] == 0)
			{
				continue;
			}
			FMenuWindow& Window = Windows[static_cast<std::size_t>(Index)];
			// Keyed by window and piece, so a growing window grows smoothly on the display.
			uint32 Key = MenuBlendKey::Windows + static_cast<uint32>(Index) * 0x10;
			uint32 ScrollBarKeys = 13;
			for (FMenuPrimitive& Quad : Window.Corners)
			{
				Quad.BlendKey = Key++;
				AddProjected(Quad, Window.bOnLayer, Window.Entry);
			}
			if (Window.bScrollBar)
			{
				for (FMenuPrimitive& Quad : Window.ScrollBar)
				{
					Quad.BlendKey = MenuBlendKey::Windows + static_cast<uint32>(Index) * 0x10 + ScrollBarKeys++;
					AddProjected(Quad, Window.bOnLayer, Window.Entry);
				}
			}
			for (FMenuPrimitive& Quad : Window.Edges)
			{
				Quad.BlendKey = Key++;
				AddProjected(Quad, Window.bOnLayer, Window.Entry);
			}
			Window.Fill.BlendKey = Key;
			AddProjected(Window.Fill, Window.bOnLayer, Window.Entry);
			Table.Add(Window.Entry, Window.FillMode);
		}
	}

	void FMenuModule::CloseWindow(int32 Index)
	{
		Flags[static_cast<std::size_t>(MenuFlag::Windows + Index)] = 0;
		Flags[static_cast<std::size_t>(FirstGrowingFlag + Index)] = 0;
	}
}
