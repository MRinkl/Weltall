#include "WorldMap/WorldMapObjectDrawing.h"

#include "Graphics/GteMatrix.h"
#include "Graphics/GteVectors.h"
#include "WorldMap/WorldMapContext.h"
#include "WorldMap/WorldMapDrawingTables.h"
#include "WorldMap/WorldMapScene.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		/** A model is drawn only while its origin is nearer than this. */
		constexpr uint16 ObjectDrawDistance = 0xD80;
		/** The models' vertices are drawn at half their size. */
		constexpr int32 HalfScale = 0x800;
		/** The light every model surface receives: SetBackColor's half grey, times 16 as the GTE holds it. */
		constexpr int32 AmbientLight = 0x80 << 4;
		constexpr int32 LightMode = 1;
		/**
		 * The screen the kernel's model drawing tests corners against: the world map sets none, so it is
		 * the field's (320 x 240), which leaves for it: a corner counts across below 320, down below 239.
		 */
		constexpr uint32 ClipWidth = 320;
		/** How far a 32:9 view reaches past either side of the 320 x 224 screen. */
		constexpr int32 WideViewBeyond = (224 * 32 / 9 - 320) / 2;
		constexpr uint32 ClipHeightLimit = (240u - 1) << 16;
		/** The colour the fogged draw modes fade from: the base the display's start sets. */
		constexpr FGteColor FogBase{128, 128, 128, 0};
		/** How far a depth is shifted into an ordering-table entry: an average's OTZ, and a single corner's depth. */
		constexpr int32 AverageShift = 2;
		constexpr int32 CornerShift = 4;
		constexpr uint8 RawBit = 1;

		/** The kernel's draw modes, as the flags table picks them (2 sorts by the farthest corner as 5 does, without the fog). */
		namespace DrawMode
		{
			constexpr int32 Average = 0;
			constexpr int32 Relit = 1;
			constexpr int32 Nearest = 3;
			constexpr int32 FoggedAverage = 4;
			constexpr int32 FoggedFarthest = 5;
		}

		uint32 PackedPoint(const FGteProjection& Point)
		{
			return static_cast<uint16>(Point.ScreenX) | static_cast<uint32>(static_cast<uint16>(Point.ScreenY)) << 16;
		}

		bool IsAcross(const FGteProjection& Point)
		{
			return static_cast<uint16>(Point.ScreenX) < ClipWidth;
		}

		bool IsDown(const FGteProjection& Point)
		{
			return PackedPoint(Point) < ClipHeightLimit;
		}

		/** The kernel's on-screen test: some corner at a depth neither 0 nor FFFFh, and on the screen across and down. */
		bool TriangleOnScreen(const FGte& Gte, const FGteVector& A, const FGteVector& B, const FGteVector& C)
		{
			for (const FGteVector& Corner : {A, B, C})
			{
				const FGteProjection Point = Gte.Rtps(Corner);
				if (static_cast<uint16>(Point.ScreenZ + 1) >= 2 && IsDown(Point) && IsAcross(Point))
				{
					return true;
				}
			}
			return false;
		}

		int16 Midway(int16 From, int16 To)
		{
			return static_cast<int16>(From + static_cast<int16>((int32{To} - int32{From}) / 2));
		}

		/**
		 * The kernel's box test with both its parts: three triangles across the part's box (its diagonal to its
		 * centre, and two faces' diagonals), then five between its edges' midpoints; hidden when no corner
		 * of any is on the screen.
		 */
		bool IsBoxHidden(const FGte& Gte, const FMdlPart& Part)
		{
			const int16 X0 = Part.BoundsMin[0], Y0 = Part.BoundsMin[1], Z0 = Part.BoundsMin[2];
			const int16 X1 = Part.BoundsMax[0], Y1 = Part.BoundsMax[1], Z1 = Part.BoundsMax[2];
			const std::array<std::array<FGteVector, 3>, 7> Triangles{{
				{{{X0, Y0, Z0}, {X1, Y1, Z1}, {Midway(X0, X1), Midway(Y0, Y1), Midway(Z0, Z1)}}},
				{{{X1, Y0, Z0}, {X0, Y1, Z0}, {X1, Y1, Z0}}},
				{{{X0, Y0, Z1}, {X1, Y0, Z1}, {X0, Y1, Z1}}},
				{{{Midway(X0, X1), Y0, Z0}, {X0, Y0, Midway(Z0, Z1)}, {X0, Midway(Y0, Y1), Z0}}},
				{{{X1, Midway(Y1, Y0), Z0}, {Midway(X1, X0), Y1, Z0}, {X1, Y1, Midway(Z0, Z1)}}},
				{{{X0, Y1, Midway(Z1, Z0)}, {X0, Midway(Y1, Y0), Z1}, {Midway(X0, X1), Y1, Z1}}},
				{{{X1, Y0, Midway(Z1, Z0)}, {Midway(X1, X0), Y0, Z1}, {X1, Midway(Y0, Y1), Z1}}},
			}};
			for (const std::array<FGteVector, 3>& Triangle : Triangles)
			{
				if (TriangleOnScreen(Gte, Triangle[0], Triangle[1], Triangle[2]))
				{
					return false;
				}
			}
			return true;
		}

		int32 Negate(int32 Value)
		{
			return static_cast<int32>(0u - static_cast<uint32>(Value));
		}

		/**
		 * One polygon through a kernel handler of the draw mode; linked into the frame when it is drawn. HorizonBucket: for an
		 * object drawn as the original whatever the culling, the horizon band's entry, past which the band covers it.
		 */
		void DrawPolygon(const FMdlPart& Part, const FMdlPrimitive& Primitive, const FWorldMapModelPolygon& Built, const FWorldMapObject& Object,
			int32 Mode, const FWorldMapDrawState& State, const FGte& Gte, EWorldMapCulling Culling, std::optional<uint16> HorizonBucket, bool bVehicle,
			FWorldMapFrameBuilder& Builder)
		{
			const FMdlPrimitiveTraits& Traits = Primitive.Traits();
			// Only the flat textured kinds have handlers of their own for the fogged modes; the others draw those as the plain average.
			const bool bFogged = (Mode == DrawMode::FoggedAverage || Mode == DrawMode::FoggedFarthest) && Traits.bTextured && !Traits.bGouraud;
			if (!bFogged && (Mode == DrawMode::FoggedAverage || Mode == DrawMode::FoggedFarthest))
			{
				Mode = DrawMode::Average;
			}
			const int32 Count = Traits.Corners;
			std::array<FGteProjection, 4> Corners{};
			for (int32 Corner = 0; Corner < Count; ++Corner)
			{
				Corners[static_cast<std::size_t>(Corner)] = Gte.Rtps(Part.Vertices[Primitive.Vertices[static_cast<std::size_t>(Corner)]]);
			}
			const std::span<const FGteProjection> Used(Corners.data(), static_cast<std::size_t>(Count));
			// Tested on the original's screen, or left whole for the display to drop where it faces away (as the terrain's).
			const bool bScreenTests = Culling == EWorldMapCulling::Original || Object.bOriginalScreenTests;
			if (bScreenTests)
			{
				// The handlers read the GTE's leading-zero count where they would test its flag, so no projection fault drops a polygon.
				// Under the other cullings the screen's width reaches as far as the widest view.
				const int32 Beyond = Culling == EWorldMapCulling::Original ? 0 : WideViewBeyond;
				const bool bDown = std::any_of(Used.begin(), Used.end(), IsDown);
				const bool bAcross = std::any_of(Used.begin(), Used.end(), [Beyond](const FGteProjection& Point)
				{
					return Point.ScreenX >= -Beyond && Point.ScreenX < static_cast<int32>(ClipWidth) + Beyond;
				});
				if (!bDown || !bAcross
					|| Nclip(Corners[0].ScreenX, Corners[0].ScreenY, Corners[1].ScreenX, Corners[1].ScreenY, Corners[2].ScreenX, Corners[2].ScreenY) <= 0)
				{
					return;
				}
			}
			uint16 Bucket = 0;
			if (Mode == DrawMode::Average || Mode == DrawMode::FoggedAverage)
			{
				const uint16 Depth = Count == 4 ? Gte.AverageDepth4(Corners[0].ScreenZ, Corners[1].ScreenZ, Corners[2].ScreenZ, Corners[3].ScreenZ)
					: Gte.AverageDepth3(Corners[0].ScreenZ, Corners[1].ScreenZ, Corners[2].ScreenZ);
				if (Depth == 0)
				{
					return;
				}
				const auto Sorted = [&State, &Corners](std::size_t Corner) { return WorldMapSortDepth(Corners[Corner].ScreenZ, State); };
				const uint16 Average = Count == 4 ? Gte.AverageDepth4(Sorted(0), Sorted(1), Sorted(2), Sorted(3)) : Gte.AverageDepth3(Sorted(0), Sorted(1), Sorted(2));
				Bucket = static_cast<uint16>(Average >> AverageShift);
			}
			else
			{
				uint16 Depth = Corners[0].ScreenZ;
				for (const FGteProjection& Corner : Used)
				{
					Depth = Mode == DrawMode::Nearest ? std::min(Depth, Corner.ScreenZ) : std::max(Depth, Corner.ScreenZ);
				}
				if (Depth == 0)
				{
					return;
				}
				Bucket = static_cast<uint16>(WorldMapSortDepth(Depth, State) >> CornerShift);
			}
			// The other cullings draw the sky behind everything: what the original's band would have covered is left out.
			if (Culling != EWorldMapCulling::Original && Object.bOriginalScreenTests && HorizonBucket && Bucket > *HorizonBucket)
			{
				return;
			}

			FWorldMapPolygon& Polygon = Builder.Add(Bucket);
			Polygon.Layer = EWorldMapLayer::Model;
			Polygon.bVehicle = bVehicle;
			Polygon.bOneSided = !bScreenTests;
			Polygon.CornerCount = static_cast<uint8>(Count);
			Polygon.bGouraud = Traits.bGouraud;
			Polygon.bTextured = Traits.bTextured;
			uint8 Code = Built.Packet.Code;
			std::array<FMdlColor, 4> Colors = Built.Packet.Colors;
			FMdlTextureState Texture = Built.Texture;
			if (Object.Look && Traits.bTextured && !Traits.bGouraud)
			{
				// The packets a cutscene rewrote: modulated by its colour of this buffer, semi-transparent or not, from its page and CLUT.
				const FWorldMapPacketLook& Look = *Object.Look;
				const std::size_t Buffer = static_cast<std::size_t>(State.DrawBuffer & 1);
				const std::array<uint8, 3>& Colour = Look.Colours[Buffer];
				Code = static_cast<uint8>((Code & ~(RawBit | 2)) | (Look.bSemiTransparent && !Look.bOpaque[Buffer] ? 2 : 0));
				Colors.fill({Colour[0], Colour[1], Colour[2]});
				Texture.TexturePage = Look.Page;
				Texture.Clut = Look.Clut.value_or(Texture.Clut);
			}
			if (bFogged)
			{
				// The last corner's depth cue fades the base colour toward the fog's, which then modulates the texels.
				const FGteColor Faded = Gte.DepthCueColor(FogBase, Corners[static_cast<std::size_t>(Count - 1)].DepthCue);
				Colors.fill({Faded.R, Faded.G, Faded.B});
				Code = static_cast<uint8>(Code & ~RawBit);
			}
			Polygon.bRaw = Traits.bTextured && (Code & RawBit) != 0;
			Polygon.bSemiTransparent = (Code & 2) != 0;
			uint8 LowU = 0xFF;
			uint8 LowV = 0xFF;
			uint8 HighU = 0;
			uint8 HighV = 0;
			for (std::size_t Corner = 0; Corner < static_cast<std::size_t>(Count); ++Corner)
			{
				Polygon.View[Corner] = ViewPoint(Gte.Transform, Part.Vertices[Primitive.Vertices[Corner]]);
				Polygon.ScreenX[Corner] = Corners[Corner].ScreenX;
				Polygon.ScreenY[Corner] = Corners[Corner].ScreenY;
				const FMdlColor& Color = Colors[Traits.bGouraud ? Corner : 0];
				Polygon.Colors[Corner] = {Color.R, Color.G, Color.B};
				if (Traits.bTextured)
				{
					Polygon.U[Corner] = Built.Uvs[Corner].U;
					Polygon.V[Corner] = Built.Uvs[Corner].V;
					Polygon.TexCoords[Corner] = {static_cast<float>(Polygon.U[Corner]), static_cast<float>(Polygon.V[Corner])};
					LowU = std::min(LowU, Polygon.U[Corner]);
					LowV = std::min(LowV, Polygon.V[Corner]);
					HighU = std::max(HighU, Polygon.U[Corner]);
					HighV = std::max(HighV, Polygon.V[Corner]);
				}
			}
			if (Traits.bTextured)
			{
				Polygon.TexturePage = Texture.TexturePage;
				Polygon.Clut = Texture.Clut;
				Polygon.TexelMin = {LowU, LowV};
				Polygon.TexelMax = {HighU, HighV};
			}
		}
	}

	TResult<FWorldMapModelPackets> BuildWorldMapModelPackets(const FWorldMapScene& Scene, const FWorldMapDrawingTables& Tables)
	{
		FGte Lighting;
		Lighting.LightMatrix = Tables.LightMatrix;
		Lighting.ColorMatrix = Tables.ColorMatrix;
		Lighting.BackColor = {AmbientLight, AmbientLight, AmbientLight};
		FWorldMapModelPackets Packets;
		FMdlTextureState State;
		for (const FWorldMapPlacement& Placement : Scene.Placements)
		{
			if (Placement.Part < 0 || static_cast<std::size_t>(Placement.Part) >= Scene.Models.Parts.size())
			{
				return MakeError("a world map placement names model part " + std::to_string(Placement.Part) + ", which the scene lacks");
			}
			const FMdlPart& Part = Scene.Models.Parts[static_cast<std::size_t>(Placement.Part)];
			for (const FMdlPrimitive& Primitive : Part.Primitives)
			{
				if (Primitive.Type == EMdlPrimitiveType::EnvironmentMappedTriangle)
				{
					return MakeError("world map model part " + std::to_string(Placement.Part) + " holds an environment-mapped polygon");
				}
			}
			TResult<std::vector<FMdlPacket>> Built = BuildMdlPackets(Part, LightMode, Lighting);
			if (!Built)
			{
				return MakeError("world map model part " + std::to_string(Placement.Part) + ": " + Built.Error().Message);
			}
			std::vector<FWorldMapModelPolygon>& Polygons = Packets.Objects.emplace_back();
			for (std::size_t Index = 0; Index < Part.Primitives.size(); ++Index)
			{
				Polygons.push_back({(*Built)[Index], Part.Primitives[Index].Texture(State), Part.Primitives[Index].Uvs});
			}
			State = Part.TextureStateAfter(State);
		}
		return Packets;
	}

	std::optional<FError> ApplyStrayHiddenWrite(const FWorldMapScene& Scene, int32 PastModels, FWorldMapModelPackets& Packets)
	{
		// The heap's header between two blocks, a flat textured quad's packet and its last texel's place in it.
		constexpr int32 BlockHeader = 8;
		constexpr int32 QuadPacket = 0x28;
		constexpr int32 LastTexel = 0x24;
		const int32 Offset = PastModels - BlockHeader;
		if (Offset >= 0 && !Scene.Placements.empty() && !Packets.Objects.empty()
			&& static_cast<std::size_t>(Scene.Placements[0].Part) < Scene.Models.Parts.size())
		{
			const FMdlPart& Part = Scene.Models.Parts[static_cast<std::size_t>(Scene.Placements[0].Part)];
			const std::size_t Index = static_cast<std::size_t>(Offset / QuadPacket);
			const auto IsFlatTexturedQuad = [](const FMdlPrimitive& Primitive)
			{
				const FMdlPrimitiveTraits& Traits = Primitive.Traits();
				return Traits.Corners == 4 && Traits.bTextured && !Traits.bGouraud;
			};
			if (Index < Part.Primitives.size() && Index < Packets.Objects[0].size() && Offset % QuadPacket == LastTexel
				&& std::all_of(Part.Primitives.begin(), Part.Primitives.begin() + static_cast<std::ptrdiff_t>(Index) + 1, IsFlatTexturedQuad))
			{
				Packets.Objects[0][Index].Uvs[3] = {1, 0};
				return std::nullopt;
			}
		}
		return MakeError("a world map scene of " + std::to_string(Scene.Placements.size())
			+ " models: the start's hide of the fifth writes where the port does not follow it");
	}

	void DrawWorldMapObjects(std::span<const FWorldMapObject> Objects, const FWorldMapScene& Scene, const FWorldMapModelPackets& Packets,
		const FWorldMapDrawingTables& Tables, const FWorldMapGround& Ground, const FWorldMapDrawState& State, EWorldMapCulling Culling,
		std::size_t VehicleObjects, FWorldMapFrameBuilder& Builder)
	{
		const int32 Reach = WorldMapReach(Culling);
		FGte Gte = WorldMapDrawingGte(State, Reach);
		for (std::size_t Channel = 0; Channel < 3; ++Channel)
		{
			Gte.FarColor[Channel] = int32{Tables.FogColor[Channel]} << 4;
		}
		const std::optional<uint16> HorizonBucket = WorldMapHorizonBucket(Tables, State);
		const std::size_t Count = std::min(Objects.size(), Packets.Objects.size());
		for (std::size_t Index = 0; Index < Count; ++Index)
		{
			const FWorldMapObject& Object = Objects[Index];
			if (Object.Hidden != 0 || Object.Flags < 0 || static_cast<std::size_t>(Object.Flags) >= Tables.DrawModes.size())
			{
				continue;
			}
			// The table's modes for the flags the placements hold; the relighting mode is not among them.
			const int32 Mode = Tables.DrawModes[static_cast<std::size_t>(Object.Flags)];
			if (Mode < DrawMode::Average || Mode > DrawMode::FoggedFarthest || Mode == DrawMode::Relit
				|| static_cast<std::size_t>(Object.Part) >= Scene.Models.Parts.size())
			{
				continue;
			}
			// Its rotation and place, turned and moved by each object it hangs from, the translation 16 bits a component each time.
			FGteMatrix Placed;
			Placed.M = Object.Matrix.M;
			Placed.T = {Object.X, Object.Y, Negate(Object.Z)};
			for (int32 Parent = Object.Parent; Parent >= 0 && static_cast<std::size_t>(Parent) < Objects.size(); Parent = Objects[static_cast<std::size_t>(Parent)].Parent)
			{
				const FWorldMapObject& Holder = Objects[static_cast<std::size_t>(Parent)];
				FGteMatrix Turn = Holder.Matrix;
				Turn.T = {Holder.X, Holder.Y, Negate(Holder.Z)};
				Placed.M = MulMatrix(Turn, Placed).M;
				Placed.T = RotTrans(Turn, {static_cast<int16>(Placed.T[0]), static_cast<int16>(Placed.T[1]), static_cast<int16>(Placed.T[2])});
			}
			int32 X = Placed.T[0] - (State.Target.X >> 12);
			int32 Z = Negate(State.Target.Z >> 12) - Placed.T[2];
			Ground.WrapDeltaUnits(X, Z);
			Placed.T = {X, Placed.T[1], Negate(Z)};
			Placed = ScaleColumns(Placed, {HalfScale, HalfScale, HalfScale});
			Gte.Transform = CompMatrix(State.Camera, Placed);
			const FGteProjection Origin = Gte.Rtps({0, 0, 0});
			if (Origin.ScreenZ >= ObjectDrawDistance * Reach + State.EyeBack || (Culling == EWorldMapCulling::Original && (Origin.Flag & GteFlag::Error) != 0))
			{
				continue;
			}
			const FMdlPart& Part = Scene.Models.Parts[static_cast<std::size_t>(Object.Part)];
			if (Culling == EWorldMapCulling::Original && IsBoxHidden(Gte, Part))
			{
				continue;
			}
			Builder.Anchor(EWorldMapAnchorKind::Model, static_cast<uint32>(Index), Gte.Transform);
			const std::vector<FWorldMapModelPolygon>& Built = Packets.Objects[Index];
			for (std::size_t Primitive = 0; Primitive < Part.Primitives.size() && Primitive < Built.size(); ++Primitive)
			{
				DrawPolygon(Part, Part.Primitives[Primitive], Built[Primitive], Object, Mode, State, Gte, Culling, HorizonBucket, Index < VehicleObjects, Builder);
			}
		}
	}
}
