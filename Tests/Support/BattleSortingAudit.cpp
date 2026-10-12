#include "Support/BattleSortingAudit.h"

#include "Battle/Show/BattleObjectsFrame.h"
#include "Battle/Show/BattleShowObjects.h"
#include "Gears/GearPackets.h"
#include "Graphics/Gte.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace Weltall::Testing
{
	namespace
	{
		/** The GPU's code bits: raw texture, semi-transparent. */
		constexpr uint32 RawCode = 1;
		constexpr uint32 SemiTransparentCode = 2;
		/** A colour code's top byte: bit 24 raw texture, bit 25 semi-transparent. */
		constexpr uint32 RawBit = 1u << 24;
		constexpr uint32 SemiTransparentBit = 1u << 25;
		constexpr int32 ScreenLines = 224;

		std::array<float, 3> ToFloats(const FMdlColor& Color)
		{
			return {static_cast<float>(Color.R), static_cast<float>(Color.G), static_cast<float>(Color.B)};
		}

		std::array<float, 3> ToFloats(uint32 ColorCode)
		{
			return {static_cast<float>(ColorCode & 0xFF), static_cast<float>((ColorCode >> 8) & 0xFF), static_cast<float>((ColorCode >> 16) & 0xFF)};
		}

		/** The GPU code bits a colour code's top byte holds. */
		uint32 CodeOf(uint32 ColorCode)
		{
			return ((ColorCode & RawBit) != 0 ? RawCode : 0) | ((ColorCode & SemiTransparentBit) != 0 ? SemiTransparentCode : 0);
		}

		bool IsLayer(const FBattleShowLink& Link)
		{
			return Link.Kind == EBattleShowLink::Fade || Link.Kind == EBattleShowLink::Hud;
		}

		/** How many of the frame's links its scene draws: those before its first layer (the fade, the HUD). */
		std::size_t SceneLinks(const FBattleShowFrame& Frame)
		{
			return static_cast<std::size_t>(std::find_if(Frame.Order.begin(), Frame.Order.end(), IsLayer) - Frame.Order.begin());
		}

		/**
		 * Over a 3D object's polygons a shadow is drawn only this close to its floor (FieldSceneRendering.cpp's): a gear's
		 * soles; over the stage sorted by depth, as high as under a gear's broad shadow.
		 */
		constexpr float OverModelShadowHeight = 16.0f;
		constexpr float BroadShadowHeight = 256.0f;

		/** The battle's sorting as the options judge it (FBattleSortingAuditOptions::Sorting). */
		class FBattleSortingJudge final : public ISortingJudge
		{
		public:
			explicit FBattleSortingJudge(EFieldSorting InSorting) : Sorting(InSorting) {}

			/**
			 * Under either depth sorting: a sprite against anything (it stands at its entry's depth, its shadow kept off it), a
			 * sprite's shadow anywhere (a card at its feet's depth, flattened on the screen: the depth says nothing of it), and the
			 * 3D objects among themselves; with the scenery sorted too, the stage's pieces among themselves (beside the 3D
			 * objects, which they are drawn over in the order); but never a blend drawn before what covers it (Sortable false).
			 */
			bool IsDecided(const FSortingPiece& Over, const FSortingPiece& Under, bool bSortable) const override
			{
				if (Sorting == EFieldSorting::PaintersOrder)
				{
					return false;
				}
				if (Over.Kind == ESortingPiece::Sprite || Under.Kind == ESortingPiece::Sprite)
				{
					return true;
				}
				if (Over.Kind == ESortingPiece::Shadow && Under.Kind != ESortingPiece::GearPart)
				{
					return true;
				}
				const bool bGears = Over.Kind == ESortingPiece::GearPart && Under.Kind == ESortingPiece::GearPart;
				const bool bStage = Sorting == EFieldSorting::DepthBufferWithScenery && Over.Kind == ESortingPiece::Scenery && Under.Kind == ESortingPiece::Scenery;
				return bSortable && (bGears || bStage);
			}

			/**
			 * A shadow is drawn over a 3D object's polygons only where they stand close to its floor, and over the stage sorted by
			 * depth where it stands no higher than a broad shadow's height: there it counts by that height.
			 */
			float BlendGap(const FSortingPiece& Blend, const FSortingShape& Shape, const FSortingPiece& Under, const std::array<float, 3>& PointUnder,
				float Gap) const override
			{
				const bool bShadow = Blend.Kind == ESortingPiece::Shadow || Blend.Kind == ESortingPiece::GearShadow;
				const bool bOverStage = Sorting == EFieldSorting::DepthBufferWithScenery && Under.Kind == ESortingPiece::Scenery;
				if (Sorting == EFieldSorting::PaintersOrder || !bShadow || (Under.Kind != ESortingPiece::GearPart && !bOverStage))
				{
					return Gap;
				}
				const float Highest = Under.Kind == ESortingPiece::GearPart ? OverModelShadowHeight : BroadShadowHeight;
				const float Height = HeightAboveFloor(Shape, PointUnder);
				return Height <= Highest ? Height : 0.0f;
			}

		private:
			EFieldSorting Sorting;
		};

		/** The world's up in the frame's view space (its y runs down: the view's second column turned the other way). */
		std::array<float, 3> FloorUpOf(const FGteMatrix& View)
		{
			const std::array<float, 3> Up = {-static_cast<float>(View.M[0][1]), -static_cast<float>(View.M[1][1]), -static_cast<float>(View.M[2][1])};
			const float Length = std::sqrt(Up[0] * Up[0] + Up[1] * Up[1] + Up[2] * Up[2]);
			return Length > 0.0f ? std::array<float, 3>{Up[0] / Length, Up[1] / Length, Up[2] / Length} : std::array<float, 3>{0.0f, -1.0f, 0.0f};
		}

		/** The frame's scene as the audit draws it, link by link, as BattleFrameDraw.cpp lays it out for the GPU. */
		class FBattleShapes
		{
		public:
			explicit FBattleShapes(const FBattleShowFrame& InFrame)
				: Frame(InFrame), Distance(static_cast<float>(std::max(InFrame.ProjectionDistance, 1))), FloorUp(FloorUpOf(InFrame.View))
			{
			}

			void Describe(const FBattleShowLink& Link, FSortingPiece& Piece, FSortingShape& Shape)
			{
				Piece.Entry = Link.Entry;
				switch (Link.Kind)
				{
				case EBattleShowLink::Quad:
					QuadShape(Frame.Quads[Link.Index], Piece, Shape);
					break;
				case EBattleShowLink::Triangle:
					TriangleShape(Frame.Triangles[Link.Index], Piece, Shape);
					break;
				case EBattleShowLink::Tile:
					TileShape(Frame.Tiles[Link.Index], Piece, Shape);
					break;
				case EBattleShowLink::Line:
					LineShape(Frame.Lines[Link.Index], Piece, Shape);
					break;
				case EBattleShowLink::ScreenQuad:
					ScreenQuadShape(Frame.ScreenQuads[Link.Index], Piece, Shape);
					break;
				case EBattleShowLink::ModelPolygon:
					ModelPolygonShape(Frame.ModelPolygons[Link.Index], Piece, Shape);
					break;
				case EBattleShowLink::Object:
					if (Frame.Objects != nullptr)
					{
						ObjectShape(Frame.Objects->Links[Link.Index], Piece, Shape);
					}
					break;
				case EBattleShowLink::Backdrop:
				case EBattleShowLink::Fade:
				case EBattleShowLink::Hud:
					break;
				}
				if (Piece.Kind == ESortingPiece::Skipped)
				{
					Shape.Count = 0;
				}
				if (Piece.Kind == ESortingPiece::Shadow || Piece.Kind == ESortingPiece::GearShadow)
				{
					// As the port draws a shadow over the 3D objects: its floor faces the world's up through its corner.
					Shape.FloorPoint = Shape.Corners[0].View;
					Shape.FloorUp = FloorUp;
				}
				// An untextured semi-transparent polygon blends as the last textured one, or the last drawing mode, set the GPU.
				if (Shape.Count != 0 && (Shape.bTextured || bSetsBlend))
				{
					Current = Shape.Blend;
				}
				else
				{
					Shape.Blend = Current;
				}
				bSetsBlend = false;
			}

		private:
			uint32 KeyOf(uint16 Drawer) const { return Drawer < Frame.Drawers.size() ? Frame.Drawers[Drawer].BlendKey : 0; }

			void QuadShape(const FBattleShowQuad& Entry, FSortingPiece& Piece, FSortingShape& Shape) const
			{
				const FSpriteQuad& Quad = Entry.Quad;
				const bool bShadow = Entry.Drawer < Frame.Drawers.size() && Frame.Drawers[Entry.Drawer].Kind == EBattleShowDrawer::Shadow;
				Piece.Kind = bShadow ? ESortingPiece::Shadow : ESortingPiece::Sprite;
				Piece.Owner = KeyOf(Entry.Drawer);
				Shape.Count = 4;
				for (std::size_t Corner = 0; Corner < 4; ++Corner)
				{
					FSortingCorner& Out = Shape.Corners[Corner];
					Out.View = Quad.ViewCorner(Corner);
					Out.U = (Corner & 1) != 0 ? Quad.U1 : Quad.U0;
					Out.V = (Corner & 2) != 0 ? Quad.V1 : Quad.V0;
					Out.Color = ToFloats(Quad.ColorCode);
				}
				if (bShadow)
				{
					FlattenShadow(Shape);
				}
				SetSortingTexture(Shape, CodeOf(Quad.ColorCode), Quad.TexturePage, Quad.Clut, true);
				BoundSortingTexels(Shape, true);
			}

			/** A sprite's shadow as the GPU draws it: once projected, its first two corners take their average height on the screen, and the last two theirs. */
			void FlattenShadow(FSortingShape& Shape) const
			{
				for (const std::array<std::size_t, 2>& Edge : {std::array<std::size_t, 2>{0, 1}, std::array<std::size_t, 2>{2, 3}})
				{
					std::array<float, 3>& A = Shape.Corners[Edge[0]].View;
					std::array<float, 3>& B = Shape.Corners[Edge[1]].View;
					if (A[2] <= 0.0f || B[2] <= 0.0f)
					{
						continue;
					}
					const float Screen = (A[1] * Distance / A[2] + B[1] * Distance / B[2]) / 2.0f;
					A[1] = Screen * A[2] / Distance;
					B[1] = Screen * B[2] / Distance;
				}
			}

			void TriangleShape(const FBattleShowTriangle& Triangle, FSortingPiece& Piece, FSortingShape& Shape) const
			{
				Piece.Kind = ESortingPiece::Particle;
				Piece.Owner = KeyOf(Triangle.Drawer);
				Shape.Count = 3;
				for (std::size_t Corner = 0; Corner < 3; ++Corner)
				{
					Shape.Corners[Corner].View = ViewPoint(Triangle.Matrix, Triangle.Corners[Corner]);
					Shape.Corners[Corner].Color = ToFloats(Triangle.ColorCodes[Corner]);
				}
				Shape.bSemiTransparent = (CodeOf(Triangle.ColorCodes[0]) & SemiTransparentCode) != 0;
			}

			/** A spawned sprite's dot or square: flat on the screen about its place's pixel, at its place's depth. */
			void TileShape(const FBattleShowTile& Tile, FSortingPiece& Piece, FSortingShape& Shape)
			{
				const std::array<float, 3> Centre = ViewPoint(Frame.View, Tile.Place);
				if (Centre[2] <= 0.0f)
				{
					return;
				}
				const float Pixel = Centre[2] / Distance;
				float Side = Pixel;
				if (!Tile.bDot)
				{
					const FGteVector Along{static_cast<int16>(Tile.Place[0] + Tile.Size), Tile.Place[1], Tile.Place[2]};
					const std::array<float, 3> Edge = ViewPoint(Frame.View, Along);
					if (Edge[2] > 0.0f)
					{
						Side = std::max(std::abs(Edge[0] / Edge[2] - Centre[0] / Centre[2]) * Distance, 1.0f) * Pixel;
					}
				}
				const float Left = Centre[0] + Pixel / 2.0f - Side / 2.0f;
				const float Top = Centre[1] + Pixel / 2.0f - Side / 2.0f;
				Piece.Kind = ESortingPiece::Particle;
				Piece.Owner = KeyOf(Tile.Drawer);
				Shape.Count = 4;
				for (std::size_t Corner = 0; Corner < 4; ++Corner)
				{
					Shape.Corners[Corner].View = {Left + ((Corner & 1) != 0 ? Side : 0.0f), Top + ((Corner & 2) != 0 ? Side : 0.0f), Centre[2]};
					Shape.Corners[Corner].Color = ToFloats(Tile.ColorCode);
				}
				Shape.bSemiTransparent = (CodeOf(Tile.ColorCode) & SemiTransparentCode) != 0;
				Shape.Blend = static_cast<EBlendMode>(Tile.Blend & 3);
				bSetsBlend = true;
			}

			/** A spawned sprite's line: a pixel of the original's screen wide, from one end to the other, each at its depth. */
			void LineShape(const FBattleShowLine& Line, FSortingPiece& Piece, FSortingShape& Shape)
			{
				const std::array<float, 3> From = ViewPoint(Line.Matrix, Line.Ends[0]);
				const std::array<float, 3> To = ViewPoint(Line.Matrix, Line.Ends[1]);
				if (From[2] <= 0.0f || To[2] <= 0.0f)
				{
					return;
				}
				const std::array<float, 2> Start = {From[0] / From[2] * Distance + 0.5f, From[1] / From[2] * Distance + 0.5f};
				const std::array<float, 2> End = {To[0] / To[2] * Distance + 0.5f, To[1] / To[2] * Distance + 0.5f};
				std::array<float, 2> Along = {End[0] - Start[0], End[1] - Start[1]};
				const float Length = std::sqrt(Along[0] * Along[0] + Along[1] * Along[1]);
				Along = Length > 1e-8f ? std::array<float, 2>{Along[0] / Length, Along[1] / Length} : std::array<float, 2>{1.0f, 0.0f};
				const std::array<float, 2> Side = {-Along[1] * 0.5f, Along[0] * 0.5f};
				const std::array<std::array<float, 2>, 4> Screen = {{
					{Start[0] - Along[0] * 0.5f - Side[0], Start[1] - Along[1] * 0.5f - Side[1]},
					{Start[0] - Along[0] * 0.5f + Side[0], Start[1] - Along[1] * 0.5f + Side[1]},
					{End[0] + Along[0] * 0.5f - Side[0], End[1] + Along[1] * 0.5f - Side[1]},
					{End[0] + Along[0] * 0.5f + Side[0], End[1] + Along[1] * 0.5f + Side[1]},
				}};
				const std::array<float, 4> Depths = {From[2], From[2], To[2], To[2]};
				Piece.Kind = ESortingPiece::Particle;
				Piece.Owner = KeyOf(Line.Drawer);
				Shape.Count = 4;
				for (std::size_t Corner = 0; Corner < 4; ++Corner)
				{
					Shape.Corners[Corner].View = {Screen[Corner][0] * Depths[Corner] / Distance, Screen[Corner][1] * Depths[Corner] / Distance, Depths[Corner]};
					Shape.Corners[Corner].Color = ToFloats(Line.ColorCode);
				}
				Shape.bSemiTransparent = (CodeOf(Line.ColorCode) & SemiTransparentCode) != 0;
				if (Line.Blend >= 0)
				{
					Shape.Blend = static_cast<EBlendMode>(Line.Blend & 3);
					bSetsBlend = true;
				}
			}

			/** A quad placed on the original's screen (a span of a script's trail): each corner where it lies there, at its end's depth. */
			void ScreenQuadShape(const FBattleShowScreenQuad& Quad, FSortingPiece& Piece, FSortingShape& Shape) const
			{
				if (Quad.Depths[0] <= 0 || Quad.Depths[1] <= 0)
				{
					return;
				}
				Piece.Kind = ESortingPiece::Particle;
				Piece.Owner = KeyOf(Quad.Drawer);
				Shape.Count = 4;
				for (std::size_t Corner = 0; Corner < 4; ++Corner)
				{
					const float Depth = static_cast<float>(Quad.Depths[Corner >> 1]);
					FSortingCorner& Out = Shape.Corners[Corner];
					Out.View = {static_cast<float>(Quad.Corners[Corner][0] - Frame.CentreX) * Depth / Distance,
						static_cast<float>(Quad.Corners[Corner][1] - Frame.CentreY) * Depth / Distance, Depth};
					Out.U = (Corner & 1) != 0 ? Quad.U1 : Quad.U0;
					Out.V = (Corner & 2) != 0 ? Quad.V1 : Quad.V0;
					Out.Color = ToFloats(Quad.ColorCode);
				}
				SetSortingTexture(Shape, CodeOf(Quad.ColorCode), Quad.TexturePage, Quad.Clut, true);
				BoundSortingTexels(Shape, true);
			}

			/** A polygon of a model a sprite draws, about the model's own centre on the screen when that is not the frame's. */
			void ModelPolygonShape(const FBattleShowModelPolygon& Polygon, FSortingPiece& Piece, FSortingShape& Shape)
			{
				const FBattleShowModel& Model = Frame.Models[Polygon.Model];
				const float Across = static_cast<float>(Model.CentreX - Frame.CentreX) / Distance;
				const float Lower = static_cast<float>(Model.CentreY - Frame.CentreY) / Distance;
				Piece.Kind = ESortingPiece::SpriteModel;
				Piece.Owner = Polygon.Model;
				Shape.Count = Polygon.CornerCount;
				for (std::size_t Corner = 0; Corner < Polygon.CornerCount; ++Corner)
				{
					const std::array<float, 3> Point = ViewPoint(Model.Matrix, Polygon.Corners[Corner]);
					FSortingCorner& Out = Shape.Corners[Corner];
					Out.View = {Point[0] + Across * Point[2], Point[1] + Lower * Point[2], Point[2]};
					Out.U = Polygon.Uvs[Corner].U;
					Out.V = Polygon.Uvs[Corner].V;
					Out.Color = ToFloats(Polygon.ColorCodes[Polygon.bGouraud ? Corner : 0]);
				}
				SetSortingTexture(Shape, CodeOf(Polygon.ColorCodes[0]), Polygon.TexturePage, Polygon.Clut, Polygon.bTextured);
				if (Polygon.bTextured)
				{
					BoundSortingTexels(Shape, false);
				}
				else if (Polygon.Blend >= 0)
				{
					Shape.Blend = static_cast<EBlendMode>(Polygon.Blend & 3);
					bSetsBlend = true;
				}
			}

			void ObjectShape(const FBattleObjectLink& Link, FSortingPiece& Piece, FSortingShape& Shape) const
			{
				const FBattleObjectsFrame& Objects = *Frame.Objects;
				switch (Link.Kind)
				{
				case EBattleObjectDraw::Polygon:
					PartShape(Objects, Objects.Polygons[Link.Index], Piece, Shape);
					break;
				case EBattleObjectDraw::Shadow:
					ShadowShape(Objects.Shadows[Link.Index], Piece, Shape);
					break;
				case EBattleObjectDraw::Cloth:
					ClothShape(Objects, Objects.Cloths[Link.Index], Piece, Shape);
					break;
				case EBattleObjectDraw::Trail:
					TrailShape(Objects, Objects.Trails[Link.Index], Piece, Shape);
					break;
				}
			}

			/** A polygon of a drawn part: the stage's (its slot's) or a 3D object's, through the matrix the GTE drew it with. */
			static void PartShape(const FBattleObjectsFrame& Objects, const FBattleDrawnPolygon& Drawn, FSortingPiece& Piece, FSortingShape& Shape)
			{
				const FBattleDrawnPart& Part = Objects.Parts[Drawn.Part];
				const FMdlPrimitive& Primitive = Part.Part->Primitives[Drawn.Number];
				const FMdlPrimitiveTraits& Traits = Primitive.Traits();
				Piece.Kind = Part.Slot == FBattleObjects::StageSlot ? ESortingPiece::Scenery : ESortingPiece::GearPart;
				Piece.Owner = Part.Slot;
				const bool bMapped = Primitive.Type == EMdlPrimitiveType::EnvironmentMappedTriangle;
				Shape.Count = Traits.Corners;
				for (std::size_t Corner = 0; Corner < Traits.Corners; ++Corner)
				{
					FSortingCorner& Out = Shape.Corners[Corner];
					Out.View = ViewPoint(Part.Matrix, Part.Part->Vertices[Primitive.Vertices[Corner]]);
					const FMdlUv& Uv = bMapped ? Drawn.Texels[Corner] : Primitive.Uvs[Corner];
					Out.U = Uv.U;
					Out.V = Uv.V;
					Out.Color = ToFloats(Drawn.Colors[Traits.bGouraud ? Corner : 0]);
				}
				const FMdlTextureState& Texture = Part.Packets->Textures[Drawn.Number];
				SetSortingTexture(Shape, Part.Packets->Packets[Drawn.Number].Code, Texture.TexturePage, Texture.Clut, Traits.bTextured);
				BoundSortingTexels(Shape, false);
			}

			static void ShadowShape(const FBattleDrawnShadow& Shadow, FSortingPiece& Piece, FSortingShape& Shape)
			{
				using namespace BattleObjectShadow;
				Piece.Kind = ESortingPiece::GearShadow;
				Piece.Owner = Shadow.Slot;
				Shape.Count = 4;
				for (std::size_t Corner = 0; Corner < 4; ++Corner)
				{
					FSortingCorner& Out = Shape.Corners[Corner];
					Out.View = ViewPoint(Shadow.Matrix, Shadow.Corners[Corner]);
					Out.U = (Corner & 1) != 0 ? LastTexel : FirstTexel;
					Out.V = (Corner & 2) != 0 ? LastTexel : FirstTexel;
					Out.Color = ToFloats(Shadow.Colour);
				}
				SetSortingTexture(Shape, Code, TexturePage, Clut, true);
				BoundSortingTexels(Shape, false);
			}

			static void ClothShape(const FBattleObjectsFrame& Objects, const FGearDrawnClothTriangle& Triangle, FSortingPiece& Piece, FSortingShape& Shape)
			{
				Piece.Kind = ESortingPiece::GearPart;
				Piece.Owner = Triangle.Slot;
				Shape.Count = 3;
				for (std::size_t Corner = 0; Corner < 3; ++Corner)
				{
					FSortingCorner& Out = Shape.Corners[Corner];
					Out.View = ViewPoint(Objects.View, Triangle.Points[Corner]);
					Out.U = Triangle.Uvs[Corner].U;
					Out.V = Triangle.Uvs[Corner].V;
					Out.Color = ToFloats(Triangle.Colors[Corner]);
				}
				SetSortingTexture(Shape, GearClothCode, Triangle.TexturePage, Triangle.Clut, true);
				BoundSortingTexels(Shape, false);
			}

			/** A trail's quad: on the screen at the projection's distance (kind 0), or in the world through the view. */
			void TrailShape(const FBattleObjectsFrame& Objects, const FBattleTrailQuad& Quad, FSortingPiece& Piece, FSortingShape& Shape) const
			{
				using namespace BattleTrailQuad;
				Piece.Kind = ESortingPiece::Particle;
				Piece.Owner = Quad.Segment;
				Shape.Count = 4;
				static constexpr std::array<uint8, 4> Columns = {0, 0, LastTexel, LastTexel};
				for (std::size_t Corner = 0; Corner < 4; ++Corner)
				{
					const FGteVector& At = Quad.Corners[Corner];
					FSortingCorner& Out = Shape.Corners[Corner];
					Out.View = Quad.Kind == 0 ? std::array<float, 3>{static_cast<float>(At[0] - Frame.CentreX), static_cast<float>(At[1] - Frame.CentreY),
													static_cast<float>(Frame.ProjectionDistance)}
											  : ViewPoint(Objects.View, At);
					Out.U = Columns[Corner];
					Out.V = Row;
					Out.Color = ToFloats(FMdlColor{Quad.Colour[0], Quad.Colour[1], Quad.Colour[2]});
				}
				SetSortingTexture(Shape, Quad.bSemiTransparent ? SemiTransparentCode : Code, TexturePage, Clut, true);
				BoundSortingTexels(Shape, false);
			}

			const FBattleShowFrame& Frame;
			float Distance = 1.0f;
			std::array<float, 3> FloorUp{};
			EBlendMode Current = EBlendMode::Average;
			/** The piece described last sets the blend mode with a drawing mode of its own (a spark, a line, an untextured model's polygon). */
			bool bSetsBlend = false;
		};

		const char* DrawerName(EBattleShowDrawer Kind)
		{
			switch (Kind)
			{
			case EBattleShowDrawer::Sprite:
				return "sprite";
			case EBattleShowDrawer::Shadow:
				return "shadow";
			case EBattleShowDrawer::Number:
				return "number";
			case EBattleShowDrawer::TotalBurst:
				return "burst";
			case EBattleShowDrawer::Total:
				return "total";
			case EBattleShowDrawer::Marker:
				return "marker";
			case EBattleShowDrawer::Tile:
				return "tile";
			case EBattleShowDrawer::Line:
				return "line";
			case EBattleShowDrawer::Trail:
				return "trail";
			case EBattleShowDrawer::Model:
				return "model";
			case EBattleShowDrawer::Extension:
				return "extension";
			}
			return "drawer";
		}
	}

	FSortingFrameAudit AuditBattleSorting(const FBattleShowFrame& Frame, const FBattleSortingAuditOptions& Options)
	{
		const std::size_t Links = SceneLinks(Frame);
		std::vector<FSortingShape> Shapes(Links);
		std::vector<FSortingPiece> Pieces(Links);
		FBattleShapes Described(Frame);
		for (std::size_t Number = 0; Number < Links; ++Number)
		{
			Described.Describe(Frame.Order[Number], Pieces[Number], Shapes[Number]);
		}
		FSortingScreen Screen;
		Screen.CenterX = static_cast<float>(Frame.CentreX);
		Screen.CenterY = static_cast<float>(Frame.CentreY);
		Screen.Lines = ScreenLines;
		Screen.Projection = static_cast<float>(std::max(Frame.ProjectionDistance, 1));
		Screen.Width = Options.Width;
		Screen.Scale = Options.Scale;
		const uint32 Background = Frame.bCleared
			? static_cast<uint32>(Frame.ClearColor[0]) << 16 | static_cast<uint32>(Frame.ClearColor[1]) << 8 | Frame.ClearColor[2]
			: 0;
		return AuditSorting(std::move(Pieces), Shapes, *Frame.Vram, Screen, Background, Options.Margin, FBattleSortingJudge(Options.Sorting));
	}

	std::string DescribeBattleSortingPiece(const FBattleShowFrame& Frame, uint32 Number, const FSortingPiece& Piece)
	{
		const FBattleShowLink& Link = Frame.Order[Number];
		std::string Text = SortingPieceName(Piece.Kind);
		const auto Drawer = [&Frame](uint16 Index) { return Index < Frame.Drawers.size() ? std::string(DrawerName(Frame.Drawers[Index].Kind)) : std::string("drawer"); };
		switch (Link.Kind)
		{
		case EBattleShowLink::Quad:
			Text += " " + Drawer(Frame.Quads[Link.Index].Drawer) + " (key " + std::to_string(Piece.Owner) + ")";
			break;
		case EBattleShowLink::Triangle:
			Text += " marker (key " + std::to_string(Piece.Owner) + ")";
			break;
		case EBattleShowLink::Tile:
			Text += " tile (key " + std::to_string(Piece.Owner) + ")";
			break;
		case EBattleShowLink::Line:
			Text += " line (key " + std::to_string(Piece.Owner) + ")";
			break;
		case EBattleShowLink::ScreenQuad:
			Text += " trail (key " + std::to_string(Piece.Owner) + ")";
			break;
		case EBattleShowLink::ModelPolygon:
			Text += " " + std::to_string(Piece.Owner) + " primitive " + std::to_string(Frame.ModelPolygons[Link.Index].Primitive);
			break;
		case EBattleShowLink::Object:
		{
			const FBattleObjectLink& Object = Frame.Objects->Links[Link.Index];
			switch (Object.Kind)
			{
			case EBattleObjectDraw::Polygon:
			{
				const FBattleDrawnPolygon& Drawn = Frame.Objects->Polygons[Object.Index];
				const FBattleDrawnPart& Part = Frame.Objects->Parts[Drawn.Part];
				Text += (Piece.Kind == ESortingPiece::Scenery ? std::string() : " slot " + std::to_string(Part.Slot)) + " bone " + std::to_string(Part.Bone)
					+ " primitive " + std::to_string(Drawn.Number);
				break;
			}
			case EBattleObjectDraw::Shadow:
				Text += " slot " + std::to_string(Piece.Owner);
				break;
			case EBattleObjectDraw::Cloth:
				Text += " slot " + std::to_string(Piece.Owner) + " cloth " + std::to_string(Object.Index);
				break;
			case EBattleObjectDraw::Trail:
				Text += " object trail " + std::to_string(Piece.Owner);
				break;
			}
			break;
		}
		default:
			break;
		}
		return Text + " entry " + std::to_string(Link.Entry);
	}
}
