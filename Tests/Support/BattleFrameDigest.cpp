#include "Support/BattleFrameDigest.h"

#include "Battle/Show/BattleObjectsFrame.h"
#include "Battle/Show/BattleShowFrame.h"
#include "Battle/Show/BattleStageBackdrop.h"
#include "Gears/GearPackets.h"
#include "Graphics/Gte.h"

#include <array>
#include <functional>
#include <span>
#include <vector>

namespace Weltall::Testing
{
	namespace
	{
		/** What drew an object's packet, as the objects' drawing oracle numbers them. */
		enum : int32
		{
			DrawnByPart = 0,
			DrawnByShadow = 1,
			DrawnByCloth = 2,
			DrawnByTrail = 3,
		};

		/** What drew a backdrop's packet, as the stage's oracle numbers them. */
		enum : int32
		{
			DrawnByPanorama = 0,
			DrawnByDome = 1,
		};

		constexpr uint8 DrawModeCode = 0xE1;
		constexpr uint8 FlatCode = 0x28;
		constexpr uint8 GradedCode = 0x38;
		constexpr uint8 SemiTransparent = 0x02;
		constexpr uint8 RawTexturedCode = 0x2D;

		/** A frame's drawing in the program's digest, and each kind of link's mark in it. */
		constexpr uint8 DrawingCode = 0xFA;
		constexpr uint8 HudMark = 0xF1;
		constexpr uint8 ObjectMark = 0xF2;
		constexpr uint8 BackdropMark = 0xF3;
		constexpr uint8 ShowMark = 0xF4;
		/** A packet the show's oracle has no form for: the entry, the length and the bytes the GPU takes of it. */
		constexpr uint8 OtherMark = 0xF5;
		/** A quad's code's bit for raw texels, in its colour word. */
		constexpr uint32 RawTexels = 1u << 24;

		/** The colours a packet of the kind holds that the digest takes: one for the flat kinds, one a corner for G3 and GT3. */
		int32 ColorsDigested(const FMdlPrimitive& Primitive)
		{
			switch (Primitive.Type)
			{
			case EMdlPrimitiveType::StaticTexturedTriangle:
			case EMdlPrimitiveType::EnvironmentMappedTriangle:
			case EMdlPrimitiveType::GouraudQuad:
			case EMdlPrimitiveType::GouraudTexturedQuad:
			case EMdlPrimitiveType::StaticGouraudQuad:
			case EMdlPrimitiveType::StaticGouraudTexturedQuad:
				return 0;
			default:
				return Primitive.Traits().bGouraud ? Primitive.Traits().Corners : 1;
			}
		}

		void DigestCorners(FOracleDigest& Digest, FGte Gte, const FGteMatrix& Matrix, std::span<const FGteVector> Corners)
		{
			Gte.Transform = Matrix;
			for (const FGteVector& Corner : Corners)
			{
				const FGteProjection Projected = Gte.Rtps(Corner);
				Digest.I32(Projected.ScreenX, Projected.ScreenY);
			}
		}

		void DigestColors(FOracleDigest& Digest, std::span<const FMdlColor> Colors)
		{
			for (const FMdlColor& Color : Colors)
			{
				Digest.I32(Color.R, Color.G, Color.B);
			}
		}

		/** Each corner's texel, the CLUT after the first and the page after the second. */
		void DigestTexels(FOracleDigest& Digest, std::span<const FMdlUv> Uvs, uint16 Clut, uint16 TexturePage)
		{
			for (std::size_t Corner = 0; Corner < Uvs.size(); ++Corner)
			{
				Digest.I32(Uvs[Corner].U | Uvs[Corner].V << 8);
				if (Corner == 0)
				{
					Digest.I32(Clut);
				}
				else if (Corner == 1)
				{
					Digest.I32(TexturePage);
				}
			}
		}

		void DigestPolygon(FOracleDigest& Digest, const FBattleObjectsFrame& Frame, const FGte& Gte, const FBattleObjectLink& Link)
		{
			const FBattleDrawnPolygon& Polygon = Frame.Polygons[Link.Index];
			const FBattleDrawnPart& Part = Frame.Parts[Polygon.Part];
			const FMdlPrimitive& Primitive = Part.Part->Primitives[Polygon.Number];
			const std::size_t CornerCount = Primitive.Traits().Corners;
			Digest.I32(DrawnByPart, Part.Slot, Part.Bone, Polygon.Number, Link.Entry, Part.Packets->Packets[Polygon.Number].Code);
			std::array<FGteVector, 4> Corners{};
			for (std::size_t Corner = 0; Corner < CornerCount; ++Corner)
			{
				Corners[Corner] = Part.Part->Vertices[Primitive.Vertices[Corner]];
			}
			DigestCorners(Digest, Gte, Part.Matrix, std::span<const FGteVector>(Corners).first(CornerCount));
			DigestColors(Digest, std::span<const FMdlColor>(Polygon.Colors).first(static_cast<std::size_t>(ColorsDigested(Primitive))));
			if (!Primitive.Traits().bTextured)
			{
				return;
			}
			const FMdlTextureState& Texture = Part.Packets->Textures[Polygon.Number];
			if (Primitive.Type == EMdlPrimitiveType::EnvironmentMappedTriangle)
			{
				DigestTexels(Digest, Polygon.Texels, Texture.Clut, Texture.TexturePage);
				return;
			}
			DigestTexels(Digest, std::span<const FMdlUv>(Primitive.Uvs).first(CornerCount), Texture.Clut, Texture.TexturePage);
		}

		void DigestShadow(FOracleDigest& Digest, const FBattleObjectsFrame& Frame, const FGte& Gte, const FBattleObjectLink& Link)
		{
			using namespace BattleObjectShadow;
			const FBattleDrawnShadow& Shadow = Frame.Shadows[Link.Index];
			Digest.I32(DrawnByShadow, Shadow.Slot, 0, 0, Link.Entry, Code);
			DigestCorners(Digest, Gte, Shadow.Matrix, Shadow.Corners);
			DigestColors(Digest, std::span<const FMdlColor>(&Shadow.Colour, 1));
			const std::array<FMdlUv, 4> Uvs{{{FirstTexel, FirstTexel}, {LastTexel, FirstTexel}, {FirstTexel, LastTexel}, {LastTexel, LastTexel}}};
			DigestTexels(Digest, Uvs, Clut, TexturePage);
		}

		void DigestCloth(FOracleDigest& Digest, const FBattleObjectsFrame& Frame, const FGte& Gte, const FBattleObjectLink& Link)
		{
			const FGearDrawnClothTriangle& Triangle = Frame.Cloths[Link.Index];
			Digest.I32(DrawnByCloth, Triangle.Slot, Triangle.Cloth, Triangle.Triangle, Link.Entry, GearClothCode);
			DigestCorners(Digest, Gte, Frame.View, Triangle.Points);
			DigestColors(Digest, Triangle.Colors);
			DigestTexels(Digest, Triangle.Uvs, Triangle.Clut, Triangle.TexturePage);
		}

		void DigestTrail(FOracleDigest& Digest, const FBattleObjectsFrame& Frame, const FGte& Gte, const FBattleObjectLink& Link)
		{
			using namespace BattleTrailQuad;
			const FBattleTrailQuad& Quad = Frame.Trails[Link.Index];
			Digest.I32(DrawnByTrail, 0, Quad.Segment, 0, Link.Entry, Quad.bSemiTransparent ? SemiTransparentCode : Code);
			if (Quad.Kind == 0)
			{
				for (const FGteVector& Corner : Quad.Corners)
				{
					Digest.I32(Corner[0], Corner[1]);
				}
			}
			else
			{
				DigestCorners(Digest, Gte, Frame.View, Quad.Corners);
			}
			Digest.I32(Quad.Colour[0], Quad.Colour[1], Quad.Colour[2]);
			const std::array<FMdlUv, 4> Uvs{{{0, Row}, {0, Row}, {LastTexel, Row}, {LastTexel, Row}}};
			DigestTexels(Digest, Uvs, Clut, TexturePage);
		}

		/** libgte's texture window command for a rectangle within its page (none: the drawing environment's, all zeros). */
		uint32 WindowWord(const std::array<int16, 4>& Window)
		{
			const auto Bits = [](int32 Value) { return static_cast<uint32>((Value & 0xFF) >> 3); };
			return 0xE2000000u | Bits(Window[1]) << 15 | Bits(Window[0]) << 10 | Bits(~(Window[3] - 1)) << 5 | Bits(~(Window[2] - 1));
		}

		void DigestBackdropQuad(FOracleDigest& Digest, int32 Owner, const FFieldBackdropQuad& Quad, bool bSemiTransparent)
		{
			switch (Quad.Kind)
			{
			case FFieldBackdropQuad::EKind::Flat:
				Digest.I32(Owner, FlatCode | (bSemiTransparent ? SemiTransparent : 0), Quad.Colors[0][0], Quad.Colors[0][1], Quad.Colors[0][2]);
				for (const std::array<int16, 2>& Corner : Quad.Corners)
				{
					Digest.I32(Corner[0], Corner[1]);
				}
				break;
			case FFieldBackdropQuad::EKind::Graded:
				Digest.I32(Owner, GradedCode | (bSemiTransparent ? SemiTransparent : 0));
				for (std::size_t Corner = 0; Corner < Quad.Corners.size(); ++Corner)
				{
					Digest.I32(Quad.Colors[Corner][0], Quad.Colors[Corner][1], Quad.Colors[Corner][2], Quad.Corners[Corner][0], Quad.Corners[Corner][1]);
				}
				break;
			case FFieldBackdropQuad::EKind::Textured:
				Digest.I32(Owner, RawTexturedCode);
				for (std::size_t Corner = 0; Corner < Quad.Corners.size(); ++Corner)
				{
					Digest.I32(Quad.Corners[Corner][0], Quad.Corners[Corner][1], Quad.Texels[Corner][0] | Quad.Texels[Corner][1] << 8);
					if (Corner == 0)
					{
						Digest.I32(Quad.Clut);
					}
					else if (Corner == 1)
					{
						Digest.I32(Quad.TexturePage);
					}
				}
				break;
			}
		}

		/** The show's projection for a quad or a triangle of its own: the frame's centre and distance. */
		FGte ShowProjection(const FBattleShowFrame& Frame, const FGteMatrix& Matrix)
		{
			FGte Gte;
			Gte.OffsetX = Frame.CentreX << 16;
			Gte.OffsetY = Frame.CentreY << 16;
			Gte.ProjectionDistance = static_cast<uint16>(Frame.ProjectionDistance);
			Gte.Transform = Matrix;
			return Gte;
		}

		/** A packet's digested bytes as the oracle lists them: its kind by its mark, then its 32-bit words or its bytes. */
		std::string PacketLine(std::span<const uint8> Packet)
		{
			constexpr char Digits[] = "0123456789abcdef";
			const uint8 Mark = Packet[0];
			std::string Line = Mark == HudMark ? "hud" : Mark == ObjectMark ? "object" : Mark == BackdropMark ? "backdrop" : Mark == OtherMark ? "other" : "show";
			if (Mark == ShowMark || Mark == OtherMark)
			{
				Line += ' ';
				for (const uint8 Byte : Packet.subspan(Mark == OtherMark ? 5 : 1))
				{
					Line += Digits[Byte >> 4];
					Line += Digits[Byte & 0xF];
				}
				return Line;
			}
			for (std::size_t At = 1; At + 4 <= Packet.size(); At += 4)
			{
				const uint32 Word = static_cast<uint32>(Packet[At]) | static_cast<uint32>(Packet[At + 1]) << 8 | static_cast<uint32>(Packet[At + 2]) << 16
					| static_cast<uint32>(Packet[At + 3]) << 24;
				Line += ' ';
				bool bLeading = true;
				for (int32 Shift = 28; Shift >= 0; Shift -= 4)
				{
					const uint32 Digit = Word >> Shift & 0xF;
					if (Digit != 0 || !bLeading || Shift == 0)
					{
						Line += Digits[Digit];
						bLeading = false;
					}
				}
			}
			return Line;
		}

		/** A projected quad's corners on the screen, as the GTE gives them to the packet. */
		std::array<std::array<int16, 2>, 4> Project(const FBattleShowFrame& Frame, const FSpriteQuad& Quad)
		{
			const FGte Gte = ShowProjection(Frame, Quad.Matrix);
			std::array<std::array<int16, 2>, 4> Screen{};
			for (std::size_t Corner = 0; Corner < Screen.size(); ++Corner)
			{
				const FGteProjection Point = Gte.Rtps(Quad.Corners[Corner]);
				Screen[Corner] = {Point.ScreenX, Point.ScreenY};
			}
			return Screen;
		}
	}

	namespace
	{
		/**
		 * A model's polygon as the battle's model drawing packs it: the kinds the show's oracle has a form for in that form, the others
		 * (a textured triangle, a graded quad) by their entry, length and the bytes the GPU takes.
		 */
		void DigestBattleModelPolygon(FOracleDigest& Digest, const FBattleShowFrame& Frame, const FBattleShowLink& Link, const FBattleShowModelPolygon& Polygon,
			const std::function<void(uint8)>& Begin)
		{
			const FBattleShowModel& Model = Frame.Models[Polygon.Model];
			FGte Gte = ShowProjection(Frame, Model.Matrix);
			Gte.OffsetX = Model.CentreX << 16;
			Gte.OffsetY = Model.CentreY << 16;
			if (Polygon.Blend >= 0)
			{
				Begin(ShowMark);
				Digest.I16(Link.Entry);
				Digest.U8(0xE1);
				Digest.I32(0xE1000000u | static_cast<uint32>(Polygon.Blend) << 5);
			}
			const uint8 Code = static_cast<uint8>(Polygon.ColorCodes[0] >> 24);
			const bool bRaw = Polygon.bTextured && (Code & 1) != 0;
			const std::size_t Corners = Polygon.CornerCount;
			const bool bShowForm = !(Polygon.bTextured && Corners == 3) && !(Polygon.bGouraud && (Corners == 4 || Polygon.bTextured));
			const auto Colour = [&Digest, &Polygon](std::size_t Corner)
			{
				const uint32 Color = Polygon.ColorCodes[Corner];
				Digest.U8(Color, Color >> 8, Color >> 16);
			};
			if (bShowForm)
			{
				Begin(ShowMark);
				Digest.I16(Link.Entry);
				Digest.U8(Code);
				if (bRaw)
				{
					Digest.U8(0, 0, 0);
				}
				else
				{
					Colour(0);
				}
			}
			else
			{
				const std::size_t Size = 1 + Corners * (4 + (Polygon.bTextured ? 2 : 0)) + (bRaw ? 0 : (Polygon.bGouraud ? Corners : 1) * 3) + (Polygon.bTextured ? 4 : 0);
				Begin(OtherMark);
				Digest.I16(Link.Entry, static_cast<int32>(Size));
				Digest.U8(Code);
			}
			for (std::size_t Corner = 0; Corner < Corners; ++Corner)
			{
				if (!bRaw && (bShowForm ? Corner > 0 && Polygon.bGouraud : Corner == 0 || Polygon.bGouraud))
				{
					Colour(Corner);
				}
				const FGteProjection Point = Gte.Rtps(Polygon.Corners[Corner]);
				Digest.I16(Point.ScreenX, Point.ScreenY);
				if (Polygon.bTextured)
				{
					Digest.U8(Polygon.Uvs[Corner].U, Polygon.Uvs[Corner].V);
				}
			}
			if (Polygon.bTextured)
			{
				Digest.I16(Polygon.Clut, Polygon.TexturePage);
			}
		}
	}

	void DigestBattleObjectLink(FOracleDigest& Digest, const FBattleObjectsFrame& Frame, const FGte& Gte, const FBattleObjectLink& Link)
	{
		switch (Link.Kind)
		{
		case EBattleObjectDraw::Polygon:
			DigestPolygon(Digest, Frame, Gte, Link);
			break;
		case EBattleObjectDraw::Shadow:
			DigestShadow(Digest, Frame, Gte, Link);
			break;
		case EBattleObjectDraw::Cloth:
			DigestCloth(Digest, Frame, Gte, Link);
			break;
		case EBattleObjectDraw::Trail:
			DigestTrail(Digest, Frame, Gte, Link);
			break;
		}
	}

	void DigestBattleBackdrop(FOracleDigest& Digest, const FBattleBackdropFrame& Frame, const std::function<void()>& BeforePacket)
	{
		const auto Begin = [&BeforePacket]()
		{
			if (BeforePacket)
			{
				BeforePacket();
			}
		};
		for (const FBattleDomePacket& Packet : Frame.Dome)
		{
			Begin();
			if (Packet.bDrawMode)
			{
				Digest.I32(DrawnByDome, DrawModeCode, 0xE1000000u | Packet.DrawPage, Packet.Window[2] == 0 ? 0xE2000000u : WindowWord(Packet.Window));
				continue;
			}
			DigestBackdropQuad(Digest, DrawnByDome, Packet.Quad, Packet.bSemiTransparent);
		}
		std::vector<FFieldBackdropQuad> Quads;
		for (std::size_t Slot = Frame.Panoramas.size(); Slot-- > 0;)
		{
			DrawFieldBackdrop(Frame.Panoramas[Slot], 0, FieldBackdropWidth, Quads);
			for (const FFieldBackdropQuad& Quad : Quads)
			{
				Begin();
				DigestBackdropQuad(Digest, DrawnByPanorama, Quad, false);
			}
		}
	}

	void DigestBattleShowTriangle(FOracleDigest& Digest, const FBattleShowFrame& Frame, const FBattleShowLink& Link, const FBattleShowTriangle& Triangle)
	{
		// The model drawing's packet: a colour per corner of a G3.
		const FGte Gte = ShowProjection(Frame, Triangle.Matrix);
		const uint8 Code = static_cast<uint8>(Triangle.ColorCodes[0] >> 24);
		const bool bGouraud = (Code & 0xFC) == 0x30;
		Digest.I16(static_cast<uint16>(Link.Entry));
		Digest.U8(Code);
		for (std::size_t Corner = 0; Corner < 3; ++Corner)
		{
			if (Corner == 0 || bGouraud)
			{
				const uint32 Color = Triangle.ColorCodes[Corner];
				Digest.U8(Color, Color >> 8, Color >> 16);
			}
			const FGteProjection Point = Gte.Rtps(Triangle.Corners[Corner]);
			Digest.I16(Point.ScreenX, Point.ScreenY);
		}
	}

	void DigestBattleShowQuad(FOracleDigest& Digest, const FBattleShowFrame& Frame, const FBattleShowQuad& Entry)
	{
		const FSpriteQuad& Quad = Entry.Quad;
		std::array<std::array<int16, 2>, 4> Screen = Project(Frame, Quad);
		const EBattleShowDrawer Kind = Frame.Drawers[Entry.Drawer].Kind;
		std::array<std::array<uint8, 2>, 4> Texels{};
		if (Kind == EBattleShowDrawer::Sprite)
		{
			// The sprite's parts: the last texel; mirrored on the screen, a column lower (none below 0: one narrower).
			int32 First = Quad.U0;
			int32 Across = Quad.U1 - Quad.U0 - 1;
			if (Screen[3][0] < Screen[0][0] && --First < 0)
			{
				First = 0;
				--Across;
			}
			const uint8 Last = static_cast<uint8>(First + Across);
			const uint8 Bottom = static_cast<uint8>(Quad.V1 - 1);
			Texels = {{{static_cast<uint8>(First), static_cast<uint8>(Quad.V0)}, {Last, static_cast<uint8>(Quad.V0)}, {static_cast<uint8>(First), Bottom}, {Last, Bottom}}};
		}
		else if (Kind == EBattleShowDrawer::Shadow)
		{
			const uint8 Last = static_cast<uint8>(Quad.U1 - 1);
			const uint8 Bottom = static_cast<uint8>(Quad.V1 - 1);
			Texels = {{{static_cast<uint8>(Quad.U0), static_cast<uint8>(Quad.V0)}, {Last, static_cast<uint8>(Quad.V0)}, {static_cast<uint8>(Quad.U0), Bottom}, {Last, Bottom}}};
			for (const std::size_t First : {std::size_t{0}, std::size_t{2}})
			{
				const int16 Sum = static_cast<int16>(Screen[First][1] + Screen[First + 1][1]);
				Screen[First][1] = Screen[First + 1][1] = static_cast<int16>(Sum / 2);
			}
		}
		else
		{
			Texels = {{{static_cast<uint8>(Quad.U0), static_cast<uint8>(Quad.V0)}, {static_cast<uint8>(Quad.U1), static_cast<uint8>(Quad.V0)},
				{static_cast<uint8>(Quad.U0), static_cast<uint8>(Quad.V1)}, {static_cast<uint8>(Quad.U1), static_cast<uint8>(Quad.V1)}}};
		}
		Digest.I16(Quad.OtIndex);
		Digest.U8(Quad.ColorCode >> 24, Quad.ColorCode, Quad.ColorCode >> 8, Quad.ColorCode >> 16);
		for (std::size_t Corner = 0; Corner < 4; ++Corner)
		{
			Digest.I16(Screen[Corner][0], Screen[Corner][1]);
			Digest.U8(Texels[Corner][0], Texels[Corner][1]);
		}
		Digest.I16(Quad.Clut, Quad.TexturePage);
	}

	void DigestBattleShowFade(FOracleDigest& Digest, const FBattleShowLink& Link, const FBattleShowFade& Fade)
	{
		Digest.I16(Link.Entry);
		Digest.U8(0xE1);
		Digest.I32(0xE1000000u | static_cast<uint32>(Fade.Mode) << 5, 0);
		Digest.I16(Link.Entry);
		Digest.U8(0x2A, Fade.Color[0], Fade.Color[1], Fade.Color[2]);
		Digest.I16(-32, -32, 320, -32, -32, 240, 320, 240);
	}

	void DigestBattleFrameDrawing(FOracleDigest& Digest, const FBattleShowFrame& Frame, const FGte& Gte)
	{
		Digest.U8(DrawingCode);
		Digest.Raw(BattleFrameDrawing(Frame, Gte));
	}

	FSha1::FDigest BattleFrameDrawing(const FBattleShowFrame& Frame, const FGte& Show, std::vector<std::string>* Lines)
	{
		// The projection as the frame's drawing had it: what the frame's updates set after the draws is the next frame's.
		FGte Gte = Show;
		Gte.OffsetX = Frame.CentreX << 16;
		Gte.OffsetY = Frame.CentreY << 16;
		Gte.ProjectionDistance = static_cast<uint16>(Frame.ProjectionDistance);
		FOracleDigest Table;
		std::vector<uint8> Packet;
		if (Lines != nullptr)
		{
			Table.KeepIn(&Packet);
		}
		// A packet begins: the one before it is listed, then this one's mark.
		const auto Begin = [&](uint8 Mark)
		{
			if (Lines != nullptr && !Packet.empty())
			{
				Lines->push_back(PacketLine(Packet));
				Packet.clear();
			}
			Table.U8(Mark);
		};
		for (const FBattleShowLink& Link : Frame.Order)
		{
			switch (Link.Kind)
			{
			case EBattleShowLink::Quad:
			{
				// A raw quad's colour is not the GPU's to read: the original leaves a sheet's frame's unset.
				FBattleShowQuad Quad = Frame.Quads[Link.Index];
				if (Quad.Quad.ColorCode >> 24 == 0)
				{
					// A part nothing has built (the room a sprite gets as it takes another's look): the oracle digests its place alone.
					Begin(OtherMark);
					Table.I16(Link.Entry, 0);
					break;
				}
				if ((Quad.Quad.ColorCode & RawTexels) != 0)
				{
					Quad.Quad.ColorCode &= 0xFF000000u;
				}
				Begin(ShowMark);
				DigestBattleShowQuad(Table, Frame, Quad);
				break;
			}
			case EBattleShowLink::Triangle:
				Begin(ShowMark);
				DigestBattleShowTriangle(Table, Frame, Link, Frame.Triangles[Link.Index]);
				break;
			case EBattleShowLink::Fade:
				// The fade's drawing mode and its tile are two packets.
				Begin(ShowMark);
				Table.I16(Link.Entry);
				Table.U8(0xE1);
				Table.I32(0xE1000000u | static_cast<uint32>(Frame.Fade.Mode) << 5, 0);
				Begin(ShowMark);
				Table.I16(Link.Entry);
				Table.U8(0x2A, Frame.Fade.Color[0], Frame.Fade.Color[1], Frame.Fade.Color[2]);
				Table.I16(-32, -32, 320, -32, -32, 240, 320, 240);
				break;
			case EBattleShowLink::Tile:
			{
				// The drawing mode, linked after the tile and so drawn before it; then the dot (a pixel) or the square.
				const FBattleShowTile& Tile = Frame.Tiles[Link.Index];
				Begin(ShowMark);
				Table.I16(Link.Entry);
				Table.U8(0xE1);
				Table.I32(0xE1000000u | static_cast<uint32>(Tile.Blend) << 5);
				Begin(ShowMark);
				Table.I16(Link.Entry);
				Table.U8(Tile.ColorCode >> 24, Tile.ColorCode, Tile.ColorCode >> 8, Tile.ColorCode >> 16);
				FGte Projection = Gte;
				Projection.Transform = Frame.View;
				const FGteProjection Point = Projection.Rtps(Tile.Place);
				if (Tile.bDot)
				{
					Table.I16(Point.ScreenX, Point.ScreenY);
					break;
				}
				// A tile: as wide as a point its size along x lies from it on the screen, a pixel at least.
				const FGteVector Along{static_cast<int16>(Tile.Place[0] + Tile.Size), Tile.Place[1], Tile.Place[2]};
				int32 Side = Projection.Rtps(Along).ScreenX - Point.ScreenX;
				Side = Side == 0 ? 1 : Side < 0 ? -Side : Side;
				const int16 Half = static_cast<int16>(Side / 2);
				Table.I16(static_cast<int16>(Point.ScreenX - Half), static_cast<int16>(Point.ScreenY - Half), Side, Side);
				break;
			}
			case EBattleShowLink::Line:
			{
				// The drawing mode, when the line has one, then the line: its colour and both ends on the screen.
				const FBattleShowLine& Line = Frame.Lines[Link.Index];
				if (Line.Blend >= 0)
				{
					Begin(ShowMark);
					Table.I16(Link.Entry);
					Table.U8(0xE1);
					Table.I32(0xE1000000u | static_cast<uint32>(Line.Blend) << 5);
				}
				Begin(ShowMark);
				Table.I16(Link.Entry);
				Table.U8(Line.ColorCode >> 24, Line.ColorCode, Line.ColorCode >> 8, Line.ColorCode >> 16);
				FGte Projection = Gte;
				Projection.Transform = Line.Matrix;
				for (const FGteVector& End : Line.Ends)
				{
					const FGteProjection Point = Projection.Rtps(End);
					Table.I16(Point.ScreenX, Point.ScreenY);
				}
				break;
			}
			case EBattleShowLink::ScreenQuad:
			{
				const FBattleShowScreenQuad& Quad = Frame.ScreenQuads[Link.Index];
				const bool bRaw = (Quad.ColorCode & RawTexels) != 0;
				const uint32 Color = bRaw ? 0 : Quad.ColorCode;
				Begin(ShowMark);
				Table.I16(Link.Entry);
				Table.U8(Quad.ColorCode >> 24, Color, Color >> 8, Color >> 16);
				for (std::size_t Corner = 0; Corner < 4; ++Corner)
				{
					Table.I16(Quad.Corners[Corner][0], Quad.Corners[Corner][1]);
					Table.U8((Corner & 1) != 0 ? Quad.U1 : Quad.U0, (Corner & 2) != 0 ? Quad.V1 : Quad.V0);
				}
				Table.I16(Quad.Clut, Quad.TexturePage);
				break;
			}
			case EBattleShowLink::ModelPolygon:
				DigestBattleModelPolygon(Table, Frame, Link, Frame.ModelPolygons[Link.Index], Begin);
				break;
			case EBattleShowLink::Hud:
				if (!Frame.Hud.Primitives.empty())
				{
					Begin(HudMark);
				}
				break;
			case EBattleShowLink::Object:
				Begin(ObjectMark);
				DigestBattleObjectLink(Table, *Frame.Objects, Gte, Frame.Objects->Links[Link.Index]);
				break;
			case EBattleShowLink::Backdrop:
				DigestBattleBackdrop(Table, Frame.Backdrop, [&Begin]() { Begin(BackdropMark); });
				break;
			}
		}
		if (Lines != nullptr && !Packet.empty())
		{
			Lines->push_back(PacketLine(Packet));
		}
		return Table.Finish();
	}
}
