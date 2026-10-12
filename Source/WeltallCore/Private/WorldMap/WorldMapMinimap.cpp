#include "WorldMap/WorldMapDrawing.h"

#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Graphics/GteVectors.h"
#include "WorldMap/WorldMapContext.h"
#include "WorldMap/WorldMapDrawingTables.h"
#include "WorldMap/WorldMapStateLayout.h"

namespace Weltall
{
	namespace
	{
		/** The map's picture: 104 x 96 pixels at (208, 120), the 128 x 128 texels of the 4-bit page at (380h, 100h) from v 80h, averaging. */
		constexpr int16 MinimapLeft = 0xD0;
		constexpr int16 MinimapTop = 0x78;
		constexpr int16 MinimapRight = 0x137;
		constexpr int16 MinimapBottom = 0xD7;
		constexpr uint8 MinimapLastU = 0x7F;
		constexpr uint8 MinimapFirstV = 0x80;
		constexpr uint8 MinimapLastV = 0xFF;
		constexpr uint16 MinimapPage = 0x1E;
		constexpr uint16 MinimapClut = 0x7F90;
		/** World units a pixel of the map stands for, across and down. */
		constexpr int32 UnitsAcross = 315;
		constexpr int32 UnitsDown = 341;
		/** The arrow's place on the screen before the GTE's offset: the map's corner less the screen's middle. */
		constexpr int32 ArrowLeft = 0x30;
		/** A shown place: a 2 x 2 yellow tile; the moving ones a pixel up and left of where their place falls. */
		constexpr FDrawColor DotColor{0x80, 0x80, 0x10};
		constexpr int16 DotSide = 2;
		constexpr int32 DotCount = 32;
		constexpr int32 VehicleDot = 24;
		constexpr int32 ShevatDot = 25;
		constexpr int32 ThamesDot = 26;
		/** The arrow's fan: red at the party's place, black at its rim, added to the screen. */
		constexpr FDrawColor ArrowCentre{0xFF, 0x40, 0x40};
		constexpr FDrawColor ArrowRim{0, 0, 0};

		uint16 StateWord(const FWorldMapContext& Context, std::size_t Offset)
		{
			return LittleEndian::ReadU16(Context.Game->Block(), Offset);
		}
	}

	void DrawWorldMapMinimap(const FWorldMapContext& Context, const FWorldMapDrawingTables& Tables, const FWorldMapDrawState& State, FDrawList& Minimap)
	{
		Minimap.Primitives.clear();
		// Linked at the ordering table's nearest entry in turn: the arrow, the shown places, the picture; drawn the other way round.
		FDrawPrimitive& Picture = Minimap.Primitives.emplace_back();
		const std::array<std::array<int16, 2>, 4> Corners{{{MinimapLeft, MinimapTop}, {MinimapRight, MinimapTop}, {MinimapLeft, MinimapBottom}, {MinimapRight, MinimapBottom}}};
		for (std::size_t Corner = 0; Corner < Corners.size(); ++Corner)
		{
			FDrawVertex& Vertex = Picture.Corners[Corner];
			Vertex.X = static_cast<float>(Corners[Corner][0]);
			Vertex.Y = static_cast<float>(Corners[Corner][1]);
			Vertex.U = (Corner & 1) != 0 ? MinimapLastU : 0.0f;
			Vertex.V = static_cast<float>((Corner & 2) != 0 ? MinimapLastV : MinimapFirstV);
		}
		Picture.Texture = FDrawTexture{0, FTexturePage::FromAttribute(MinimapPage), FClutPosition::FromAttribute(MinimapClut)};
		Picture.Blend = EDrawBlend::SemiTransparent;
		Picture.BlendKey = WorldMapBlendKeys::MinimapPicture;

		const uint32 Shown = LittleEndian::ReadU32(Context.Game->Block(), WorldMapStateLayout::MinimapDots) | Context.Scene->StoryDots;
		for (int32 Dot = DotCount - 1; Dot >= 0; --Dot)
		{
			if ((Shown >> Dot & 1) == 0 || (Dot == VehicleDot && Context.bVehicleAway))
			{
				continue;
			}
			int32 X = MinimapLeft + Tables.MinimapDots[static_cast<std::size_t>(Dot)][0];
			int32 Y = MinimapTop + Tables.MinimapDots[static_cast<std::size_t>(Dot)][1];
			const auto Moving = [&](std::size_t PlaceX, std::size_t PlaceZ) {
				X = MinimapLeft - 1 + StateWord(Context, PlaceX) / UnitsAcross;
				Y = MinimapTop - 1 + StateWord(Context, PlaceZ) / UnitsDown;
			};
			if (Dot == VehicleDot)
			{
				Moving(WorldMapStateLayout::VehicleX, WorldMapStateLayout::VehicleZ);
			}
			else if (Dot == ShevatDot)
			{
				Moving(WorldMapStateLayout::ShevatX, WorldMapStateLayout::ShevatZ);
			}
			else if (Dot == ThamesDot)
			{
				Moving(WorldMapStateLayout::ThamesX, WorldMapStateLayout::ThamesZ);
			}
			Minimap.Primitives.push_back(
				MakeGouraudRect({static_cast<int16>(X), static_cast<int16>(Y), DotSide, DotSide}, {DotColor, DotColor, DotColor, DotColor}));
			Minimap.Primitives.back().bGouraud = false;
			Minimap.Primitives.back().BlendKey = WorldMapBlendKeys::MinimapDots + static_cast<uint32>(Dot);
		}

		// The arrow: the party's place on the map, turned with the camera's yaw, projected at the projection's own distance.
		FGte Gte = WorldMapDrawingGte(State);
		Gte.Transform.M = RotationXyz({0, 0, State.Yaw}).M;
		Gte.Transform.T = {(Context.Controlled.X >> 12) / UnitsAcross + ArrowLeft, (Context.Controlled.Z >> 12) / UnitsDown + MinimapTop - Context.ScreenCentreY,
			State.ProjectionDistance};
		for (std::size_t Triangle = Tables.ArrowCorners.size() / 3; Triangle-- > 0;)
		{
			FDrawPrimitive& Arrow = Minimap.Primitives.emplace_back();
			Arrow.CornerCount = 3;
			Arrow.bGouraud = true;
			Arrow.Blend = EDrawBlend::SemiTransparent;
			Arrow.UntexturedBlend = EBlendMode::Add;
			Arrow.BlendKey = WorldMapBlendKeys::MinimapArrow + static_cast<uint32>(Triangle);
			for (std::size_t Corner = 0; Corner < 3; ++Corner)
			{
				const FGteProjection Point = Gte.Rtps(Tables.ArrowCorners[Triangle * 3 + Corner]);
				FDrawVertex& Vertex = Arrow.Corners[Corner];
				Vertex.X = static_cast<float>(Point.ScreenX);
				Vertex.Y = static_cast<float>(Point.ScreenY);
				Vertex.Color = Corner == 0 ? ArrowCentre : ArrowRim;
			}
		}
	}
}
