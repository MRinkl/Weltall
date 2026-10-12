#include "Menu/MenuMain.h"

namespace Weltall
{
	namespace
	{
		constexpr uint8 Free = FMenuFileScreen::Free;
		constexpr int32 PerCard = FMenuFileScreen::PerCard;
		constexpr int32 PiecesEntry = 4;
		constexpr int32 CellSize = 16;
		constexpr int32 FullScale = 0x1000;
		constexpr uint8 Neutral = 0x80;
		/** The icons' page (4-bit, at (320, 0)) and their colours' rows, one a card from 449. */
		constexpr int32 IconPageX = 0x140;
		constexpr int32 IconPageY = 0x80;
		constexpr int32 IconColoursY = 0x1C1;
		/** The cursor's grey takes away from what lies under it. */
		constexpr int32 SubtractiveBlend = 2;
		constexpr uint8 LinePoints = 3;
		/** The cards drawn with the cursor's save picked out. */
		constexpr uint8 CursorDrawn = 2;
		/** A card's orb (lit for the cursor's card) and its label, 144 pixels apart. */
		constexpr int32 OrbFrame = 0x115;
		constexpr int32 LitOrbFrame = 0x122;
		constexpr int32 FirstCardLabel = 0x162;
		constexpr int32 OrbX = 0x1E;
		constexpr int32 LabelX = 0x1B;
		constexpr int32 CardLabelY = 0x36;
		constexpr int32 CardSpacing = 0x90;
		/** The icons' animation: 15 frames a step, six steps; the cursor's grey, 4 a frame from 4 to 128 and back. */
		constexpr int32 FramesPerStep = 15;
		constexpr int32 Steps = 6;
		constexpr int32 GreyStep = 4;
		constexpr int32 GreyTop = 0x80;
		constexpr int32 GreyTurn = 0x7C;
		constexpr FDrawColor Green{0, 0xFF, 0};
		constexpr FDrawColor Red{0xFF, 0, 0};
		/** The blend keys of the cards' pieces: + cell * 8 + piece. */
		constexpr uint32 CardKeys = 0x4000;

		FGteVector LayerPoint(int32 X, int32 Y)
		{
			std::array<FGteVector, 4> Points{};
			SetLayerQuad(Points, X, Y, 0, 0);
			return Points[0];
		}
	}

	void FMenuMain::InitCardCells()
	{
		const FMenuMainTables& Tables = Resources.Main();
		for (std::size_t Cell = 0; Cell < Files.CellPieces.size(); ++Cell)
		{
			FMenuCardCell& Pieces = Files.CellPieces[Cell];
			const int32 X = Tables.CellX[Cell];
			const int32 Y = Tables.CellY[Cell];
			SetLayerQuad(Pieces.Points, X, Y, CellSize, CellSize);

			Pieces.Icon = {};
			Pieces.Icon.Code = MenuGpu::TexturedQuad;
			Pieces.Icon.Corners[0].Color = {Neutral, Neutral, Neutral};
			Pieces.Icon.Page = MenuGpu::TexturePage(0, 0, IconPageX, IconPageY);
			Pieces.Icon.Points = Pieces.Points;
			Pieces.Icon.BlendKey = CardKeys + static_cast<uint32>(Cell) * 8;

			Pieces.Highlight = {};
			Pieces.Highlight.Code = MenuGpu::FlatQuad | MenuGpu::SemiTransparent;
			Pieces.Highlight.Corners[0].Color = {Neutral, Neutral, Neutral};
			Pieces.Highlight.Points = Pieces.Points;
			Pieces.Highlight.BlendKey = CardKeys + static_cast<uint32>(Cell) * 8 + 1;
			Pieces.HighlightMode = FMenuPrimitive::MakeDrawMode(MenuGpu::TexturePage(0, SubtractiveBlend, IconPageX, IconPageY));

			// The top and right edges, then the left and bottom ones.
			const FGteVector TopLeft = LayerPoint(X, Y);
			const FGteVector TopRight = LayerPoint(X + CellSize, Y);
			const FGteVector BottomLeft = LayerPoint(X, Y + CellSize);
			const FGteVector BottomRight = LayerPoint(X + CellSize, Y + CellSize);
			for (std::size_t Edge = 0; Edge < Pieces.Outline.size(); ++Edge)
			{
				FMenuPrimitive& Line = Pieces.Outline[Edge];
				Line = {};
				Line.Code = MenuGpu::FlatPolyline;
				Line.LinePoints = LinePoints;
				Line.Corners[0].Color = Green;
				Line.Space = EMenuSpace::Layer;
				Line.Points = {TopLeft, Edge == 0 ? TopRight : BottomLeft, BottomRight, FGteVector{}};
			}
		}
	}

	void FMenuMain::DrawCards()
	{
		DrawCellHighlight();
		DrawCellIcons(0, 0, PerCard);
		DrawCellIcons(PerCard, PerCard, 2 * PerCard);
		DrawCellOutlines();
		DrawCardLabels();
		if (++Files.IconFrames == FramesPerStep)
		{
			Files.IconFrames = 0;
			if (++Files.IconStep == Steps)
			{
				Files.IconStep = 0;
			}
		}
		if (!Files.bGreyFalling)
		{
			Files.Grey += GreyStep;
			if (Files.Grey > GreyTop)
			{
				Files.bGreyFalling = true;
				Files.Grey = GreyTurn;
			}
		}
		else
		{
			Files.Grey -= GreyStep;
			if (Files.Grey < 0)
			{
				Files.bGreyFalling = false;
				Files.Grey = GreyStep;
			}
		}
	}

	void FMenuMain::DrawCellHighlight()
	{
		if (Files.Drawn != CursorDrawn)
		{
			return;
		}
		// The grey goes over every cell of the cursor's save, or over the cursor's cell alone where it is free.
		const int32 CursorAt = CursorCell();
		const uint8 File = CursorAt != FMenuFileScreen::NoCell ? Files.Cells[static_cast<std::size_t>(CursorAt)] : Free;
		for (int32 Cell = 0; Cell < 2 * PerCard; ++Cell)
		{
			if (Files.Cells[static_cast<std::size_t>(Cell)] != File || (File == Free && Cell != CursorAt))
			{
				continue;
			}
			FMenuCardCell& Pieces = Files.CellPieces[static_cast<std::size_t>(Cell)];
			const uint8 Grey = static_cast<uint8>(Files.Grey);
			Pieces.Highlight.Corners[0].Color = {Grey, Grey, Grey};
			AddProjected(Pieces.Highlight, true, PiecesEntry);
			Table.Add(PiecesEntry, Pieces.HighlightMode);
		}
	}

	void FMenuMain::DrawCellIcons(int32 FirstFile, int32 FirstCell, int32 EndFile)
	{
		int32 Cell = FirstCell;
		for (int32 Number = FirstFile; Number < EndFile; ++Number)
		{
			const FMenuStoreFile& File = Files.Files[static_cast<std::size_t>(Number)];
			if (!File.bShown)
			{
				continue;
			}
			// Each of the file's blocks shows its icon in the next cell.
			const uint8 Left = static_cast<uint8>(Number << 4);
			const uint8 Right = static_cast<uint8>((Number + 1) << 4);
			const uint8 Top = File.IconRows[static_cast<std::size_t>(Files.IconStep)];
			const uint8 Bottom = static_cast<uint8>(Top + CellSize);
			for (int32 Block = 0; Block < File.Head[3]; ++Block, ++Cell)
			{
				FMenuPrimitive& Icon = Files.CellPieces[static_cast<std::size_t>(Cell)].Icon;
				Icon.Corners[0].U = Left;
				Icon.Corners[0].V = Top;
				Icon.Corners[1].U = Right;
				Icon.Corners[1].V = Top;
				Icon.Corners[2].U = Left;
				Icon.Corners[2].V = Bottom;
				Icon.Corners[3].U = Right;
				Icon.Corners[3].V = Bottom;
				Icon.Clut = MenuGpu::Clut(Number << 4, Number / PerCard + IconColoursY);
				AddProjected(Icon, true, PiecesEntry);
			}
		}
	}

	void FMenuMain::DrawCellOutlines()
	{
		if (Files.Drawn == 0)
		{
			return;
		}
		const int32 CursorAt = CursorCell();
		const uint8 File = CursorAt != FMenuFileScreen::NoCell ? Files.Cells[static_cast<std::size_t>(CursorAt)] : Free;
		for (int32 Cell = 0; Cell < 2 * PerCard; ++Cell)
		{
			// A card's 16th cell is never drawn.
			if (Cell % PerCard == PerCard - 1)
			{
				continue;
			}
			bool bRed = Files.Cells[static_cast<std::size_t>(Cell)] == File && Files.Drawn == CursorDrawn;
			if (bRed && File == Free)
			{
				bRed = Cell == CursorAt;
			}
			FMenuCardCell& Pieces = Files.CellPieces[static_cast<std::size_t>(Cell)];
			for (std::size_t Edge = 0; Edge < Pieces.Outline.size(); ++Edge)
			{
				FMenuPrimitive& Line = Pieces.Outline[Edge];
				Line.Corners[0].Color = bRed ? Red : Green;
				for (std::size_t Point = 0; Point < LinePoints; ++Point)
				{
					const FGteProjection Projected = LayerGte.Rtps(Line.Points[Point]);
					Line.Corners[Point].X = Projected.ScreenX;
					Line.Corners[Point].Y = Projected.ScreenY;
				}
				Table.Add(PiecesEntry, Line);
			}
		}
	}

	void FMenuMain::DrawCardLabels()
	{
		if (Files.Drawn == 0)
		{
			return;
		}
		for (int32 Card = 0; Card < FMenuFileScreen::Cards; ++Card)
		{
			if (Flags[MenuFlag::Cards] == 0)
			{
				continue;
			}
			// The orb is lit while the cursor stands on the card.
			const bool bCursorHere = Flags[MenuFlag::TargetPointers] != 0 && CursorCell() != FMenuFileScreen::NoCell && CursorCell() / PerCard == Card;
			std::vector<FMenuPrimitive>& Orb = Files.Orbs[static_cast<std::size_t>(Card)];
			std::vector<FMenuPrimitive>& Label = Files.Labels[static_cast<std::size_t>(Card)];
			Orb.clear();
			Label.clear();
			Resources.Sheet().Draw(bCursorHere ? LitOrbFrame : OrbFrame, OrbX + Card * CardSpacing, CardLabelY, FullScale, Orb);
			Resources.Sheet().Draw(FirstCardLabel + Card, LabelX + Card * CardSpacing, CardLabelY, FullScale, Label);
			AddScreen(Label);
			if (!Orb.empty())
			{
				Table.Add(PiecesEntry, Orb.front());
			}
		}
	}
}
