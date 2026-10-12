#include "Battle/Show/BattleShow.h"

#include "Battle/BattleContext.h"
#include "Battle/BattleProgram.h"
#include "BattleShowDrawing.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"
#include "Graphics/SpriteDrawing.h"

#include <algorithm>
#include <array>

namespace Weltall
{
	void FBattleShow::DrawSpriteModel(FSpriteTask& Task, bool bOwnRotation)
	{
		FSprite& Sprite = Task.Sprite;
		BattleShowDrawing::UpdateMatrix(Sprite);
		if (Sprite.ModelFile == nullptr)
		{
			return;
		}
		ModelDrawing.StageLight.reset();
		if (Options.bUnlitModelsTakeStageLight && Objects.Stage != nullptr)
		{
			const std::array<uint8, 3> Ambient = Objects.Stage->AmbientColour();
			ModelDrawing.StageLight = FBattleStageLight{Objects.LightMatrix, Objects.ColourMatrix, {Ambient[0] * 16, Ambient[1] * 16, Ambient[2] * 16}};
		}
		if (!ModelDrawing.Draw(Sprite, SpriteModelMatrix(Sprite), Task.Id, bOwnRotation))
		{
			ReportFault(EBattleShowFault::Model);
		}
	}

	void FBattleShow::DrawWave(const FBattleWave& Wave)
	{
		FSprite* Sprite = Extensions.Live(Wave.Sprite);
		if (Sprite == nullptr)
		{
			return;
		}
		PartScratch.clear();
		if (!ExtensionDrawing.DrawWave(*Sprite, Wave))
		{
			ReportFault(EBattleShowFault::Extension);
			return;
		}
		AddQuads(*Wave.Sprite.Task, EBattleShowDrawer::Extension);
	}

	void FBattleShow::DrawScroll(const FBattleScroll& Scroll)
	{
		FSprite* Sprite = Extensions.Live(Scroll.Sprite);
		if (Sprite == nullptr)
		{
			return;
		}
		PartScratch.clear();
		if (!ExtensionDrawing.DrawScroll(*Sprite, Scroll))
		{
			ReportFault(EBattleShowFault::Extension);
			return;
		}
		AddQuads(*Scroll.Sprite.Task, EBattleShowDrawer::Extension);
	}

	void FBattleShow::RetintModel(const FSprite& Sprite)
	{
		if (!ModelDrawing.Retint(Sprite))
		{
			ReportFault(EBattleShowFault::Model);
		}
	}

	namespace
	{
		using namespace BattleShowDrawing;

		/** A dot's or a tile's drawing mode takes the low two bits of the sprite's blend; a line's, of the blend less one. */
		constexpr int32 BlendModes = 3;

		/** A trail's quads: raw texels of a strip of the effects' page, a column across and eight rows along a span. */
		constexpr uint8 TrailCode = 0x2D;
		constexpr uint8 SemiTransparentCode = 0x02;
		constexpr uint8 TrailFirstColumn = 0xC0;
		constexpr uint8 TrailFirstRow = 0xF0;
		constexpr uint8 TrailLastRow = 0xF7;
		constexpr uint16 TrailPage = 0x1F;
		constexpr uint16 TrailClut = 0x7BBF;
		constexpr uint32 TrailColor = 0xF0F0F0;
		/** A span lies two entries behind the trail's sprite or two before it. */
		constexpr int32 TrailEntryStep = 2;
		/** The curve's spans of a five-point trail: the first (points 0-3) and the last (points 1-4), eight steps each. */
		constexpr int32 FirstSpanKind = 0;
		constexpr int32 LastSpanKind = 4;
		constexpr uint32 TrailKeys = 0x30000000;

		/** A product of the kernel's sine or cosine and a width, in 8192ths toward zero. */
		int16 Across(int32 Turned, int32 Width)
		{
			int32 Product = Turned * Width;
			if (Product < 0)
			{
				Product += 0x1FFF;
			}
			return static_cast<int16>(Product >> 13);
		}
	}

	void FBattleShow::AddTile(const FSpriteTask& Task, bool bDot, int32 Entry)
	{
		const FSprite& Sprite = Task.Sprite;
		const uint16 Drawer = static_cast<uint16>(Shown.Drawers.size());
		Shown.Drawers.push_back({Task.Id, EBattleShowDrawer::Tile, Place(Sprite)});
		FBattleShowTile& Tile = Shown.Tiles.emplace_back();
		Tile.Place = Place(Sprite);
		// A square's side is the word a part sprite keeps its frame's top in.
		Tile.Size = bDot ? int16{0} : Sprite.FrameTop;
		Tile.bDot = bDot;
		Tile.ColorCode = ColorCode({Sprite.Red, Sprite.Green, Sprite.Blue}, Sprite.Code);
		Tile.Blend = static_cast<uint8>(Sprite.Blend() & BlendModes);
		Tile.Drawer = Drawer;
		Links.push_back({EBattleShowLink::Tile, static_cast<uint32>(Shown.Tiles.size() - 1), Entry});
	}

	void FBattleShow::AddLine(const FSpriteTask& Task, const FGteMatrix& Matrix, const FGteVector& To, int32 Entry)
	{
		const FSprite& Sprite = Task.Sprite;
		const uint16 Drawer = static_cast<uint16>(Shown.Drawers.size());
		Shown.Drawers.push_back({Task.Id, EBattleShowDrawer::Line, Place(Sprite)});
		FBattleShowLine& Line = Shown.Lines.emplace_back();
		Line.Matrix = Matrix;
		Line.Ends = {Place(Sprite), To};
		Line.ColorCode = ColorCode({Sprite.Red, Sprite.Green, Sprite.Blue}, Sprite.Code);
		// The sprite's blend, when it has one, is a drawing mode of the blend mode one below it.
		const int32 Blend = Sprite.Blend();
		Line.Blend = Blend == 0 ? int8{-1} : static_cast<int8>((Blend - 1) & BlendModes);
		Line.Drawer = Drawer;
		Links.push_back({EBattleShowLink::Line, static_cast<uint32>(Shown.Lines.size() - 1), Entry});
	}

	void FBattleShow::DrawTrail(const FBattleSprites::FTrailDrawing& Trail)
	{
		// The sprite's own matrix at its place under the view, which its points go through.
		FSprite& Sprite = *Trail.Sprite;
		UpdateMatrix(Sprite);
		Gte.Transform = SpriteDrawMatrix(Sprite, SpriteModelMatrix(Sprite), Camera.View);
		using FPoint = std::array<int32, 3>;
		// A point of the curve: four control points from First, each weighted by the span's step, the sum's quarter.
		const auto CurvePoint = [this, &Trail](int32 SpanKind, int32 Step, std::size_t First)
		{
			const std::array<int16, 4>& Weights = Context->Program->TrailCurve[static_cast<std::size_t>(SpanKind * FBattleProgramTables::TrailSteps + Step)];
			FPoint Point{};
			for (std::size_t Control = 0; Control < Weights.size(); ++Control)
			{
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					// The GTE's interpolation of a vector by a factor: 4096ths, saturated to 16 bits.
					Point[Axis] += std::clamp((int32{Weights[Control]} * Trail.Points[First + Control][Axis]) >> 12, -0x8000, 0x7FFF);
				}
			}
			return FPoint{Point[0] >> 2, Point[1] >> 2, Point[2] >> 2};
		};
		// A segment: a span from From to To as a quad across it on the screen, joined to the span before.
		const uint16 Drawer = static_cast<uint16>(Shown.Drawers.size());
		int32 Count = 0;
		std::array<std::array<int16, 2>, 2> Joint{};
		const auto Span = [&](const FPoint& From, const FPoint& To)
		{
			if (From == To)
			{
				return;
			}
			const int32 Entry = Trail.Entry + (Trail.Behind[static_cast<std::size_t>(Count >> 3)] != 0 ? TrailEntryStep : -TrailEntryStep);
			const FGteProjection Start = Gte.Rtps({static_cast<int16>(From[0]), static_cast<int16>(From[1]), static_cast<int16>(From[2])});
			const FGteProjection End = Gte.Rtps({static_cast<int16>(To[0]), static_cast<int16>(To[1]), static_cast<int16>(To[2])});
			if (!InTable(Entry))
			{
				return;
			}
			const int32 Angle = GteRatan2(End.ScreenY - Start.ScreenY, End.ScreenX - Start.ScreenX, *Arctangents) + 0x400;
			const int16 AcrossX = Across(GteCos(Angle), Trail.Width);
			const int16 AcrossY = Across(GteSin(Angle), Trail.Width);
			if (Count == 0)
			{
				Shown.Drawers.push_back({Sprite.Task != nullptr ? Sprite.Task->Id | TrailKeys : TrailKeys, EBattleShowDrawer::Trail, Place(Sprite)});
				Joint = {{{static_cast<int16>(Start.ScreenX - AcrossX), static_cast<int16>(Start.ScreenY - AcrossY)},
					{static_cast<int16>(Start.ScreenX + AcrossX), static_cast<int16>(Start.ScreenY + AcrossY)}}};
			}
			FBattleShowScreenQuad& Quad = Shown.ScreenQuads.emplace_back();
			Quad.Corners[0] = Joint[0];
			Quad.Corners[1] = Joint[1];
			Joint = {{{static_cast<int16>(End.ScreenX - AcrossX), static_cast<int16>(End.ScreenY - AcrossY)},
				{static_cast<int16>(End.ScreenX + AcrossX), static_cast<int16>(End.ScreenY + AcrossY)}}};
			Quad.Corners[2] = Joint[0];
			Quad.Corners[3] = Joint[1];
			Quad.Depths = {Start.ScreenZ, End.ScreenZ};
			Quad.U0 = static_cast<uint8>(TrailFirstColumn + (Count & 7));
			Quad.U1 = static_cast<uint8>(Quad.U0 + 1);
			Quad.V0 = TrailFirstRow;
			Quad.V1 = TrailLastRow;
			// The trail's blend goes into the page alone: the original's quad is not semi-transparent, so it is drawn opaque.
			Quad.TexturePage = static_cast<uint16>(TrailPage | (Trail.Blend != 0 ? (Trail.Blend - 1) & BlendModes : 0) << 5);
			Quad.Clut = TrailClut;
			Quad.ColorCode = TrailColor | uint32{TrailCode} << 24;
			// Blended: in the mode the page holds, the sprite's, or the averaging one when the sprite names none.
			if (Options.bTrailsBlend)
			{
				Quad.ColorCode |= uint32{SemiTransparentCode} << 24;
			}
			Quad.Drawer = Drawer;
			Links.push_back({EBattleShowLink::ScreenQuad, static_cast<uint32>(Shown.ScreenQuads.size() - 1), Entry});
			++Count;
		};
		// The curve through five points: the first span's eight steps, the last span's, then the last point itself.
		FPoint Last = CurvePoint(FirstSpanKind, 0, 0);
		for (int32 Step = 1; Step < FBattleProgramTables::TrailSteps; ++Step)
		{
			const FPoint Next = CurvePoint(FirstSpanKind, Step, 0);
			Span(Last, Next);
			Last = Next;
		}
		for (int32 Step = 0; Step < FBattleProgramTables::TrailSteps; ++Step)
		{
			const FPoint Next = CurvePoint(LastSpanKind, Step, 1);
			Span(Last, Next);
			Last = Next;
		}
		const FGteVector& End = Trail.Points.back();
		Span(Last, {End[0], End[1], End[2]});
	}
}
