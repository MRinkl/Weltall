#include "Support/WorldMapSortingAudit.h"

#include <array>
#include <utility>
#include <vector>

namespace Weltall::Testing
{
	namespace
	{
		/** A sprite's parts map their texels edge to edge: the far edges one past the last texel. */
		constexpr float EdgeHair = 1.0f / 64.0f;
		/**
		 * How far above its floor what lies under a shadow may stand for the shadow to be drawn over it where the drawing knows
		 * the floor (the arena's): a gear's broad shadow's, and over a gear (FieldSceneRendering.cpp's).
		 */
		constexpr float BroadShadowHeight = 256.0f;
		constexpr float OverModelShadowHeight = 16.0f;

		/**
		 * What the world map sorts by depth with the scenery (WorldMapFrameDraw.cpp): its ground, place models, trees and clouds;
		 * not the party's ship, which keeps the game's order against them.
		 */
		bool IsScenery(ESortingPiece Kind)
		{
			return Kind == ESortingPiece::Ground || Kind == ESortingPiece::PlaceModel || Kind == ESortingPiece::Tree || Kind == ESortingPiece::Cloud;
		}

		/** The world map's sorting as the options judge it (FWorldMapSortingAuditOptions::Sorting). */
		class FWorldMapSortingJudge final : public ISortingJudge
		{
		public:
			FWorldMapSortingJudge(const FWorldMapSortingAuditOptions& InOptions, bool bInGearsUnderGround)
				: Options(InOptions), bGearsUnderGround(bInGearsUnderGround)
			{
			}

			/**
			 * Under either depth sorting a sprite against anything, and the arena's gears among themselves (the field's models'
			 * rule); with the scenery sorted too, two of the scenery's pieces, a cloud among them; but where the gears stand
			 * under the ground, the ground keeps the order (FWorldMapFrame::bGearsUnderGround); and a blend drawn before what
			 * covers it always.
			 */
			bool IsDecided(const FSortingPiece& Over, const FSortingPiece& Under, bool bSortable) const override
			{
				if (Options.Sorting == EFieldSorting::PaintersOrder)
				{
					return false;
				}
				if (Over.Kind == ESortingPiece::Sprite || Under.Kind == ESortingPiece::Sprite)
				{
					return true;
				}
				const bool bGears = Over.Kind == ESortingPiece::GearPart && Under.Kind == ESortingPiece::GearPart;
				const bool bScenery =
					Options.Sorting == EFieldSorting::DepthBufferWithScenery && !bGearsUnderGround && IsScenery(Over.Kind) && IsScenery(Under.Kind);
				return bSortable && (bGears || bScenery);
			}

			/** Where the drawing knows the floor, a shadow is drawn over what stands no higher above it than its height: there it counts by that height. */
			float BlendGap(const FSortingPiece& Blend, const FSortingShape& Shape, const FSortingPiece& Under, const std::array<float, 3>& PointUnder,
				float Gap) const override
			{
				if (Options.Sorting == EFieldSorting::PaintersOrder || !Options.bShadowsOnFloors || Blend.Kind != ESortingPiece::Shadow)
				{
					return Gap;
				}
				const float Highest = Under.Kind == ESortingPiece::GearPart ? OverModelShadowHeight : BroadShadowHeight;
				const float Height = HeightAboveFloor(Shape, PointUnder);
				return Height <= Highest ? Height : 0.0f;
			}

		private:
			const FWorldMapSortingAuditOptions& Options;
			bool bGearsUnderGround = false;
		};

		ESortingPiece KindOf(EWorldMapLayer Layer)
		{
			switch (Layer)
			{
			case EWorldMapLayer::Terrain:
				return ESortingPiece::Ground;
			case EWorldMapLayer::Sky:
			case EWorldMapLayer::Horizon:
				return ESortingPiece::Background;
			case EWorldMapLayer::Sprite:
				return ESortingPiece::Sprite;
			case EWorldMapLayer::Model:
				return ESortingPiece::PlaceModel;
			case EWorldMapLayer::Gear:
				return ESortingPiece::GearPart;
			case EWorldMapLayer::Shadow:
				return ESortingPiece::Shadow;
			case EWorldMapLayer::Billboard:
				return ESortingPiece::Tree;
			case EWorldMapLayer::Cloud:
				return ESortingPiece::Cloud;
			case EWorldMapLayer::Particle:
			case EWorldMapLayer::Effect:
				return ESortingPiece::Particle;
			}
			return ESortingPiece::Skipped;
		}

		/** The polygon as the audit draws it; an untextured semi-transparent one blends as the textured one before it set the GPU (Blend). */
		FSortingShape ShapeOf(const FWorldMapPolygon& Polygon, EBlendMode Blend)
		{
			FSortingShape Shape;
			Shape.Count = Polygon.CornerCount;
			for (std::size_t Corner = 0; Corner < Polygon.CornerCount; ++Corner)
			{
				FSortingCorner& Out = Shape.Corners[Corner];
				Out.View = Polygon.View[Corner];
				Out.U = Polygon.TexCoords[Corner][0];
				Out.V = Polygon.TexCoords[Corner][1];
				const FDrawColor& Color = Polygon.Colors[Polygon.bGouraud ? Corner : 0];
				Out.Color = {static_cast<float>(Color.R), static_cast<float>(Color.G), static_cast<float>(Color.B)};
			}
			Shape.bTextured = Polygon.bTextured;
			Shape.bSemiTransparent = Polygon.bSemiTransparent;
			Shape.bRaw = Polygon.bRaw;
			Shape.bOneSided = Polygon.bOneSided;
			Shape.Blend = Blend;
			if (!Polygon.bTextured)
			{
				return Shape;
			}
			Shape.Page = FTexturePage::FromAttribute(Polygon.TexturePage);
			Shape.Clut = FClutPosition::FromAttribute(Polygon.Clut);
			Shape.WindowWidth = Polygon.WindowWidth;
			Shape.LowU = Polygon.WindowWidth != 0 ? 0.0f : static_cast<float>(Polygon.TexelMin[0]);
			Shape.HighU = (Polygon.WindowWidth != 0 ? static_cast<float>(Polygon.WindowWidth) : static_cast<float>(Polygon.TexelMax[0]) + 1.0f) - EdgeHair;
			Shape.LowV = static_cast<float>(Polygon.TexelMin[1]);
			Shape.HighV = static_cast<float>(Polygon.TexelMax[1]) + 1.0f - EdgeHair;
			return Shape;
		}
	}

	FSortingFrameAudit AuditWorldMapSorting(const FWorldMapFrame& Frame, const FVram& Vram, const FWorldMapSortingAuditOptions& Options)
	{
		std::vector<FSortingShape> Shapes;
		std::vector<FSortingPiece> Pieces;
		Shapes.reserve(Frame.Polygons.size());
		Pieces.reserve(Frame.Polygons.size());
		EBlendMode Current = EBlendMode::Average;
		for (const FWorldMapPolygon& Polygon : Frame.Polygons)
		{
			if (Polygon.bTextured)
			{
				Current = FTexturePage::FromAttribute(Polygon.TexturePage).Blend;
			}
			FSortingPiece& Piece = Pieces.emplace_back();
			Piece.Kind = Polygon.bVehicle ? ESortingPiece::Vehicle : KindOf(Polygon.Layer);
			Piece.Entry = Polygon.Bucket;
			if (Polygon.Anchor != FWorldMapPolygon::NoAnchor && Polygon.Anchor < Frame.Anchors.size())
			{
				Piece.Owner = Frame.Anchors[Polygon.Anchor].Key & 0xFFFFFF;
			}
			FSortingShape& Shape = Shapes.emplace_back(ShapeOf(Polygon, Current));
			if (Piece.Kind == ESortingPiece::Shadow)
			{
				SetSortingFloor(Shape);
			}
		}
		FSortingScreen Screen;
		Screen.CenterX = static_cast<float>(Frame.ScreenCenterX);
		Screen.CenterY = static_cast<float>(Frame.ScreenCenterY);
		Screen.Lines = Options.Lines;
		Screen.Projection = static_cast<float>(Frame.ProjectionDistance);
		Screen.Width = Options.Width;
		Screen.Scale = Options.Scale;
		return AuditSorting(std::move(Pieces), Shapes, Vram, Screen, Options.Clear, Options.Margin, FWorldMapSortingJudge(Options, Frame.bGearsUnderGround));
	}

	std::string DescribeWorldMapSortingPiece(const FWorldMapFrame& Frame, uint32 Number, const FSortingPiece& Piece)
	{
		std::string Text = SortingPieceName(Piece.Kind);
		switch (Piece.Kind)
		{
		case ESortingPiece::PlaceModel:
		case ESortingPiece::Vehicle:
			Text += " object " + std::to_string(Piece.Owner);
			break;
		case ESortingPiece::GearPart:
			// The arena's anchors: a gear's side, then its joint.
			Text += " side " + std::to_string(Piece.Owner >> 16) + " joint " + std::to_string(Piece.Owner & 0xFFFF);
			break;
		case ESortingPiece::Sprite:
			Text += " slot " + std::to_string(Piece.Owner);
			break;
		default:
			break;
		}
		const FWorldMapPolygon& Polygon = Frame.Polygons[Number];
		return Text + " polygon " + std::to_string(Number) + " entry " + std::to_string(Polygon.Bucket);
	}
}
