#include "Support/SortingAudit.h"

#include "Field/FieldCamera.h"
#include "Field/FieldGearDrawing.h"
#include "Gears/GearPackets.h"
#include "Graphics/MdlModel.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>
#include <vector>

namespace Weltall::Testing
{
	namespace
	{
		constexpr uint32 NoPiece = std::numeric_limits<uint32>::max();
		constexpr int32 ScreenLines = 224;
		/** The GPU's code bits: raw texture, semi-transparent. */
		constexpr uint32 RawCode = 1;
		constexpr uint32 SemiTransparentCode = 2;
		/** A sprite quad's colour code: bit 24 raw texture, bit 25 semi-transparent. */
		constexpr uint32 QuadRawBit = 1u << 24;
		constexpr uint32 QuadSemiTransparentBit = 1u << 25;
		/** The main table's entry a particle of depth mode 0 is linked into: in front of everything. */
		constexpr int32 FrontEntry = 1;

		std::array<float, 3> ToFloats(const FMdlColor& Color)
		{
			return {static_cast<float>(Color.R), static_cast<float>(Color.G), static_cast<float>(Color.B)};
		}

		std::array<float, 3> ToFloats(const std::array<int32, 3>& View)
		{
			return {static_cast<float>(View[0]), static_cast<float>(View[1]), static_cast<float>(View[2])};
		}

		bool IsShadow(ESortingPiece Kind)
		{
			return Kind == ESortingPiece::Shadow || Kind == ESortingPiece::GearShadow;
		}

		/** A 3D model's polygon: a gear's body or cloth, a child's model. */
		bool IsModel(ESortingPiece Kind)
		{
			return Kind == ESortingPiece::GearPart || Kind == ESortingPiece::SpriteModel;
		}

		/** What the port sorts by depth where a model marks the pixels: another model, the scenery, a particle (at its entry's depth). */
		bool IsSortedAgainstModels(ESortingPiece Kind)
		{
			return IsModel(Kind) || Kind == ESortingPiece::Scenery || Kind == ESortingPiece::Particle;
		}

		/** The field's sorting as the options judge it (FSortingAuditOptions::Sorting). */
		class FFieldSortingJudge final : public ISortingJudge
		{
		public:
			explicit FFieldSortingJudge(EFieldSorting InSorting) : Sorting(InSorting) {}

			/**
			 * An overlay against anything (in front by design); and under the port's per-pixel sorting a sprite against
			 * anything (the depth a sprite stands at is its entry's, not its flat quad's), a model against what it sorts
			 * there, and with the scenery sorted too two of the scenery's pieces, by depth unless it is a blend drawn before
			 * what covers it (Sortable false).
			 */
			bool IsDecided(const FSortingPiece& OverPiece, const FSortingPiece& UnderPiece, bool bSortable) const override
			{
				const ESortingPiece Over = OverPiece.Kind;
				const ESortingPiece Under = UnderPiece.Kind;
				if (Over == ESortingPiece::Overlay || Under == ESortingPiece::Overlay)
				{
					return true;
				}
				if (Sorting == EFieldSorting::PaintersOrder)
				{
					return false;
				}
				const bool bModel = (IsModel(Over) && IsSortedAgainstModels(Under)) || (IsModel(Under) && IsSortedAgainstModels(Over));
				const bool bBothScenery = Over == ESortingPiece::Scenery && Under == ESortingPiece::Scenery;
				return Over == ESortingPiece::Sprite || Under == ESortingPiece::Sprite
					|| (bSortable && (bModel || (Sorting == EFieldSorting::DepthBufferWithScenery && bBothScenery)));
			}

			/** The port draws a shadow over what stands no higher above its floor than its height: there it counts by that height. */
			float BlendGap(const FSortingPiece& Blend, const FSortingShape& Shape, const FSortingPiece& Under, const std::array<float, 3>& PointUnder,
				float Gap) const override
			{
				if (Sorting == EFieldSorting::PaintersOrder || !IsShadow(Blend.Kind))
				{
					return Gap;
				}
				float Highest = Blend.Kind == ESortingPiece::GearShadow ? PortSorting::GearShadowHeight : PortSorting::CharacterShadowHeight;
				if (IsModel(Under.Kind))
				{
					Highest = PortSorting::OverModelShadowHeight;
				}
				const float Height = HeightAboveFloor(Shape, PointUnder);
				return Height <= Highest ? Height : 0.0f;
			}

		private:
			EFieldSorting Sorting;
		};

		/** What the frame's pieces are drawn from: each placed model's first corner, each gear polygon's layout item, the scrolls. */
		struct FFrameLookup
		{
			FFrameLookup(const FFieldFrame& Frame, const FFieldScene& Scene)
			{
				ObjectCorners.reserve(Scene.Objects.size());
				uint32 Corner = 0;
				for (const FFieldSceneObject& Object : Scene.Objects)
				{
					ObjectCorners.push_back(Corner);
					Corner += static_cast<uint32>(Scene.PartOf(Object).Vertices.size());
				}
				Scrolls.assign(Scene.Objects.size(), {});
				for (const FFieldScrolledModel& Scrolled : Frame.ScrolledModels)
				{
					if (Scrolled.Object < Scrolls.size())
					{
						Scrolls[Scrolled.Object] = {Scrolled.U, Scrolled.V};
					}
				}
				GearItems.assign(Frame.Gears.Polygons.size(), NoPiece);
				for (std::size_t Item = 0; Item < Frame.Gears.Items.size(); ++Item)
				{
					const FFieldGearLayoutItem& Laid = Frame.Gears.Items[Item];
					for (uint32 Polygon = Laid.FirstPolygon; Polygon < Laid.FirstPolygon + Laid.PolygonCount && Polygon < GearItems.size(); ++Polygon)
					{
						GearItems[Polygon] = static_cast<uint32>(Item);
					}
				}
			}

			std::vector<uint32> ObjectCorners;
			std::vector<std::array<uint8, 2>> Scrolls;
			std::vector<uint32> GearItems;
		};

		void SceneryShape(const FFieldFrame& Frame, const FFieldScene& Scene, const FFrameLookup& Lookup, uint32 Index, FSortingPiece& Piece, FSortingShape& Shape)
		{
			const FFieldScenePolygon& Polygon = Scene.Polygons[Index];
			const FFieldPolygonView& Drawn = Frame.Scenery.Polygons[Index];
			Piece.Owner = Polygon.Object;
			Piece.Entry = Drawn.Bucket;
			if (Scene.Objects[Polygon.Object].bBackground)
			{
				return;
			}
			Piece.Kind = ESortingPiece::Scenery;
			const FMdlPrimitive& Primitive = Scene.PrimitiveOf(Polygon);
			const FMdlPrimitiveTraits& Traits = Primitive.Traits();
			Shape.Count = Traits.Corners;
			for (std::size_t Corner = 0; Corner < Traits.Corners; ++Corner)
			{
				FSortingCorner& Out = Shape.Corners[Corner];
				Out.View = ToFloats(Frame.Scenery.Corners[Lookup.ObjectCorners[Polygon.Object] + Primitive.Vertices[Corner]].View);
				Out.U = Primitive.Uvs[Corner].U;
				Out.V = Primitive.Uvs[Corner].V;
				Out.Color = ToFloats(Drawn.Colors[Traits.bGouraud ? Corner : 0]);
			}
			SetSortingTexture(Shape, Polygon.Packet.Code, 0, 0, Traits.bTextured);
			Shape.Page = Polygon.Page;
			Shape.Clut = Polygon.Clut;
			Shape.Blend = Traits.bTextured ? Polygon.Page.Blend : Drawn.UntexturedBlend;
			Shape.bRaw = Shape.bRaw && !Drawn.bFogged;
			Shape.ScrollU = Lookup.Scrolls[Polygon.Object][0];
			Shape.ScrollV = Lookup.Scrolls[Polygon.Object][1];
			BoundSortingTexels(Shape, false);
		}

		void CharacterShape(const FFieldFrame& Frame, uint32 Index, FSortingPiece& Piece, FSortingShape& Shape)
		{
			const FFieldCharacterQuad& Entry = Frame.Quads[Index];
			const FSpriteQuad& Quad = Entry.Quad;
			Piece.Kind = Frame.Characters[Entry.Character].bOnFloor ? ESortingPiece::Shadow : ESortingPiece::Sprite;
			Piece.Owner = Entry.Character;
			Piece.Entry = Quad.OtIndex;
			const std::array<float, 3> Color = {static_cast<float>(Quad.ColorCode & 0xFF), static_cast<float>((Quad.ColorCode >> 8) & 0xFF),
				static_cast<float>((Quad.ColorCode >> 16) & 0xFF)};
			Shape.Count = 4;
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				FSortingCorner& Out = Shape.Corners[Corner];
				Out.View = Quad.ViewCorner(Corner);
				Out.U = (Corner & 1) != 0 ? Quad.U1 : Quad.U0;
				Out.V = (Corner & 2) != 0 ? Quad.V1 : Quad.V0;
				Out.Color = Color;
			}
			SetSortingTexture(Shape, ((Quad.ColorCode & QuadRawBit) != 0 ? RawCode : 0) | ((Quad.ColorCode & QuadSemiTransparentBit) != 0 ? SemiTransparentCode : 0),
				Quad.TexturePage, Quad.Clut, true);
			// A sprite's parts may lie in VRAM's port rows (a pilot's stand-in).
			Shape.Page = FTexturePage::FromPortAttribute(Quad.TexturePage);
			Shape.Clut = FClutPosition::FromPortAttribute(Quad.Clut);
			BoundSortingTexels(Shape, true);
		}

		void ParticleShape(const FFieldFrame& Frame, uint32 Index, FSortingPiece& Piece, FSortingShape& Shape)
		{
			const FFieldParticleQuad& Quad = Frame.Particles[Index];
			Piece.Kind = Quad.OtIndex <= FrontEntry ? ESortingPiece::Overlay : ESortingPiece::Particle;
			Piece.Owner = Quad.BlendKey;
			Piece.Entry = Quad.OtIndex;
			Shape.Count = 4;
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				FSortingCorner& Out = Shape.Corners[Corner];
				Out.View = Quad.ViewCorner(Corner);
				Out.U = Quad.Texels[Corner][0];
				Out.V = Quad.Texels[Corner][1];
				Out.Color = ToFloats(FMdlColor{Quad.Color[0], Quad.Color[1], Quad.Color[2]});
			}
			SetSortingTexture(Shape, SemiTransparentCode, Quad.TexturePage, Quad.Clut, true);
			BoundSortingTexels(Shape, true);
		}

		void GearShape(const FFieldFrame& Frame, const FFrameLookup& Lookup, const FFieldDrawItem& Item, FSortingPiece& Piece, FSortingShape& Shape)
		{
			const FFieldGearsFrame& Gears = Frame.Gears;
			if (Item.Gear == EFieldGearDraw::Cloth)
			{
				const FGearDrawnClothTriangle& Triangle = Gears.Cloths[Item.Index];
				Piece.Kind = ESortingPiece::GearPart;
				Piece.Owner = Triangle.Slot;
				Piece.Entry = Triangle.Bucket;
				Shape.Count = 3;
				const auto Space = std::find_if(Gears.ClothSpaces.begin(), Gears.ClothSpaces.end(),
					[&Triangle](const FFieldGearClothSpace& Placed) { return Placed.Slot == Triangle.Slot; });
				for (std::size_t Corner = 0; Corner < 3; ++Corner)
				{
					FSortingCorner& Out = Shape.Corners[Corner];
					const FGteVector& Point = Triangle.Points[Corner];
					Out.View = ViewPoint(Frame.View.View, Point);
					if (Space != Gears.ClothSpaces.end())
					{
						// A placed model's cloth lies in its own space.
						const std::array<float, 3> Turned = Space->Space.Turn({static_cast<float>(Point[0]), static_cast<float>(Point[1]), static_cast<float>(Point[2])});
						Out.View = {Turned[0] + Space->Space.T[0], Turned[1] + Space->Space.T[1], Turned[2] + Space->Space.T[2]};
					}
					Out.U = Triangle.Uvs[Corner].U;
					Out.V = Triangle.Uvs[Corner].V;
					Out.Color = ToFloats(Triangle.Colors[Corner]);
				}
				SetSortingTexture(Shape, GearClothCode, Triangle.TexturePage, Triangle.Clut, true);
				BoundSortingTexels(Shape, false);
				return;
			}
			const uint32 ItemIndex = Item.Index < Lookup.GearItems.size() ? Lookup.GearItems[Item.Index] : NoPiece;
			if (ItemIndex == NoPiece)
			{
				return;
			}
			const FFieldGearLayoutItem& Laid = Gears.Items[ItemIndex];
			const FFieldGearObject& Object = Gears.Objects[ItemIndex];
			const FGteMatrix& Matrix = Object.Matrix;
			const FFieldGearPolygon& Drawn = Gears.Polygons[Item.Index];
			Piece.Owner = static_cast<uint32>(Laid.Slot);
			Piece.Entry = Drawn.Bucket;
			if (Laid.Packets == nullptr)
			{
				using namespace FieldGearShadow;
				Piece.Kind = ESortingPiece::GearShadow;
				const int16 X = Laid.HalfWidth;
				const int16 Z = Laid.HalfDepth;
				const std::array<FGteVector, 4> Corners = {
					FGteVector{X, 0, Z}, FGteVector{static_cast<int16>(-X), 0, Z}, FGteVector{X, 0, static_cast<int16>(-Z)},
					FGteVector{static_cast<int16>(-X), 0, static_cast<int16>(-Z)}};
				Shape.Count = 4;
				for (std::size_t Corner = 0; Corner < 4; ++Corner)
				{
					FSortingCorner& Out = Shape.Corners[Corner];
					Out.View = ViewPoint(Matrix, Corners[Corner]);
					Out.U = (Corner & 1) != 0 ? LastTexel : FirstTexel;
					Out.V = (Corner & 2) != 0 ? LastRow : FirstRow;
					Out.Color = ToFloats(Drawn.Colors[0]);
				}
				SetSortingTexture(Shape, Code, TexturePage, Clut, true);
				BoundSortingTexels(Shape, false);
				return;
			}
			Piece.Kind = ESortingPiece::GearPart;
			const uint32 Number = Item.Index - Laid.FirstPolygon;
			const FMdlPrimitive& Primitive = Laid.Part->Primitives[Number];
			const FMdlPrimitiveTraits& Traits = Primitive.Traits();
			Shape.Count = Traits.Corners;
			for (std::size_t Corner = 0; Corner < Traits.Corners; ++Corner)
			{
				FSortingCorner& Out = Shape.Corners[Corner];
				const FGteVector& Vertex = Laid.Part->Vertices[Primitive.Vertices[Corner]];
				Out.View = ViewPoint(Matrix, Vertex);
				if (Object.bFine)
				{
					// A model drawn finer than its matrix holds (a gear's stand-in at its sprite's size).
					const std::array<float, 3> Turned = Object.Fine.Turn({static_cast<float>(Vertex[0]), static_cast<float>(Vertex[1]), static_cast<float>(Vertex[2])});
					Out.View = {Turned[0] + Object.Fine.T[0], Turned[1] + Object.Fine.T[1], Turned[2] + Object.Fine.T[2]};
				}
				Out.U = Primitive.Uvs[Corner].U;
				Out.V = Primitive.Uvs[Corner].V;
				Out.Color = ToFloats(Drawn.Colors[Traits.bGouraud ? Corner : 0]);
			}
			const FMdlTextureState& Texture = Laid.Packets->Textures[Number];
			SetSortingTexture(Shape, Laid.Packets->Packets[Number].Code, Texture.TexturePage, Texture.Clut,
				Traits.bTextured && Primitive.Type != EMdlPrimitiveType::EnvironmentMappedTriangle);
			if (Laid.bPortRows)
			{
				Shape.Page = FTexturePage::FromPortAttribute(Texture.TexturePage);
				Shape.Clut = FClutPosition::FromPortAttribute(Texture.Clut);
			}
			BoundSortingTexels(Shape, false);
		}

		void SpriteModelShape(const FFieldFrame& Frame, uint32 Index, FSortingPiece& Piece, FSortingShape& Shape)
		{
			const FFieldSpriteModelPolygon& Polygon = Frame.SpriteModels.Polygons[Index];
			const FMdlPrimitiveTraits& Traits = MdlPrimitiveTraits(Polygon.Type);
			Piece.Kind = ESortingPiece::SpriteModel;
			Piece.Owner = Polygon.Model;
			Piece.Entry = Polygon.Bucket;
			Shape.Count = Polygon.CornerCount;
			for (std::size_t Corner = 0; Corner < Polygon.CornerCount; ++Corner)
			{
				FSortingCorner& Out = Shape.Corners[Corner];
				Out.View = ViewPoint(Frame.SpriteModels.Models[Polygon.Model].Matrix, Polygon.Corners[Corner]);
				Out.U = Polygon.Uvs[Corner].U;
				Out.V = Polygon.Uvs[Corner].V;
				Out.Color = ToFloats(Polygon.Colors[Traits.bGouraud ? Corner : 0]);
			}
			SetSortingTexture(Shape, Polygon.Code, Polygon.TexturePage, Polygon.Clut, Traits.bTextured);
			Shape.bRaw = Shape.bRaw && !Polygon.bFogged;
			BoundSortingTexels(Shape, false);
		}

		void Describe(const FFieldFrame& Frame, const FFieldScene& Scene, const FFrameLookup& Lookup, const FFieldDrawItem& Item, FSortingPiece& Piece,
			FSortingShape& Shape)
		{
			switch (Item.Kind)
			{
			case EFieldDrawKind::Scenery:
				SceneryShape(Frame, Scene, Lookup, Item.Index, Piece, Shape);
				break;
			case EFieldDrawKind::Character:
				CharacterShape(Frame, Item.Index, Piece, Shape);
				break;
			case EFieldDrawKind::Particle:
				ParticleShape(Frame, Item.Index, Piece, Shape);
				break;
			case EFieldDrawKind::Gear:
				GearShape(Frame, Lookup, Item, Piece, Shape);
				break;
			case EFieldDrawKind::SpriteModel:
				SpriteModelShape(Frame, Item.Index, Piece, Shape);
				break;
			case EFieldDrawKind::Backdrop:
				Piece.Entry = Frame.Backdrop.Entry;
				break;
			}
			if (Piece.Kind == ESortingPiece::Skipped)
			{
				Shape.Count = 0;
			}
			if (IsShadow(Piece.Kind))
			{
				SetSortingFloor(Shape);
			}
		}
	}

	FSortingFrameAudit AuditFieldSorting(const FFieldFrame& Frame, const FFieldScene& Scene, const FVram& Vram, const FSortingAuditOptions& Options)
	{
		const FFrameLookup Lookup(Frame, Scene);
		std::vector<FSortingShape> Shapes(Frame.Order.size());
		std::vector<FSortingPiece> Pieces(Frame.Order.size());
		for (std::size_t Number = 0; Number < Frame.Order.size(); ++Number)
		{
			Describe(Frame, Scene, Lookup, Frame.Order[Number], Pieces[Number], Shapes[Number]);
		}
		FSortingScreen Screen;
		Screen.CenterX = static_cast<float>(FieldScreenCenterX);
		Screen.CenterY = static_cast<float>(FieldScreenCenterY);
		Screen.Lines = ScreenLines;
		Screen.Projection = static_cast<float>(Frame.View.ProjectionDistance);
		Screen.Width = Options.Width;
		Screen.Scale = Options.Scale;
		const uint32 Background = static_cast<uint32>(Frame.Background.R) << 16 | static_cast<uint32>(Frame.Background.G) << 8 | Frame.Background.B;
		return AuditSorting(std::move(Pieces), Shapes, Vram, Screen, Background, Options.Margin, FFieldSortingJudge(Options.Sorting));
	}

	std::string DescribeSortingPiece(const FFieldFrame& Frame, const FFieldScene& Scene, uint32 Number, const FSortingPiece& Piece)
	{
		const FFieldDrawItem& Item = Frame.Order[Number];
		std::string Text = SortingPieceName(Piece.Kind);
		switch (Piece.Kind)
		{
		case ESortingPiece::Scenery:
		{
			const FFieldScenePolygon& Polygon = Scene.Polygons[Item.Index];
			const FFieldSceneObject& Object = Scene.Objects[Polygon.Object];
			Text += " object " + std::to_string(Polygon.Object) + " (actor " + std::to_string(Object.Actor) + ", model " + std::to_string(Object.Model)
				+ ") primitive " + std::to_string(Polygon.Primitive);
			break;
		}
		case ESortingPiece::Sprite:
		case ESortingPiece::Shadow:
			Text += " of character " + std::to_string(Piece.Owner) + " (key " + std::to_string(Frame.Characters[Piece.Owner].BlendKey) + ")";
			break;
		case ESortingPiece::GearPart:
		case ESortingPiece::GearShadow:
			Text += " slot " + std::to_string(Piece.Owner) + (Item.Gear == EFieldGearDraw::Cloth ? " cloth " : " polygon ") + std::to_string(Item.Index);
			break;
		case ESortingPiece::SpriteModel:
			Text += " " + std::to_string(Piece.Owner) + " polygon " + std::to_string(Item.Index);
			break;
		default:
			break;
		}
		return Text + " entry " + std::to_string(Piece.Entry);
	}
}
