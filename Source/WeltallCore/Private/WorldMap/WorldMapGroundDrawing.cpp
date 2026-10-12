#include "WorldMap/WorldMapGroundDrawing.h"

#include "Graphics/GteMatrix.h"
#include "Graphics/GteVectors.h"
#include "WorldMap/WorldMapContext.h"
#include "WorldMap/WorldMapScene.h"

#include <algorithm>
#include <limits>

namespace Weltall
{
	namespace
	{
		constexpr int32 ScreenWidth = 320;
		constexpr int32 ScreenHeight = 216;
		constexpr int32 BucketShift = 4;

		/** A shadow: a 32-unit square on the ground, drawn while its nearest corner is nearer than 1000h. */
		constexpr int16 ShadowHalf = 16;
		constexpr uint16 ShadowReach = 0x1000;
		constexpr int32 GearShadowScale = 0x1800;
		constexpr std::array<int32, 3> VehicleShadowScale{0x1800, 0x1000, 0x4800};
		constexpr int16 GearShadow = 1;
		constexpr int16 VehicleShadow = 2;
		/** As the scene's start builds the shadows' packets: dim, averaging, a 16 x 16 blob of the 4-bit page at (380h, 100h). */
		constexpr FDrawColor ShadowColor{0x40, 0x40, 0x48};
		constexpr uint16 ShadowPage = 0x1E;
		constexpr uint16 ShadowClut = 0x7F92;
		constexpr uint8 ShadowU = 0x80;
		constexpr uint8 ShadowV = 0xF0;
		constexpr uint8 ShadowSide = 16;

		/** A billboard: 48 x 72 units standing on its place, drawn while its foot is nearer than E00h; at most 512 a frame. */
		constexpr std::array<FGteVector, 4> BillboardCorners{{{-24, -72, 0}, {24, -72, 0}, {-24, 0, 0}, {24, 0, 0}}};
		constexpr uint16 BillboardReach = 0xE00;
		constexpr int32 BillboardLimit = 0x200;
		/** As the scene's start builds the billboards' packets: opaque, unshaded, 32 x 48 texels of the 4-bit page at (380h, 100h). */
		constexpr uint16 BillboardPage = 0x1E;
		constexpr uint8 BillboardV = 0x40;
		constexpr uint8 BillboardWidth = 32;
		constexpr uint8 BillboardHeight = 48;
		/** The shade palettes: the last 16 colours of the models' fogged rows from y 1F0h, one per 100h of the depth cue. */
		constexpr int32 ShadeRow = 0x1F0;
		constexpr int32 ShadeColumn = 0xF0;
		constexpr int32 ShadeShift = 8;
		constexpr int32 DepthCueMost = 0xFFF;
		constexpr int32 BlockUnits = 0x800;
		constexpr int32 HalfWorldLimit = 0x4000;
		constexpr int32 Outside = -1;

		bool IsOnScreen(const std::array<FGteProjection, 3>& Corners)
		{
			bool bAcross = false;
			bool bDown = false;
			for (const FGteProjection& Corner : Corners)
			{
				bAcross = bAcross || static_cast<uint16>(Corner.ScreenX) < ScreenWidth;
				bDown = bDown || static_cast<uint16>(Corner.ScreenY) < ScreenHeight;
			}
			return bAcross && bDown;
		}

		/** A quad of four corners (as the GPU takes them) with a texture page and CLUT, its texels mapped edge to edge. */
		void SetTexturedQuad(FWorldMapPolygon& Polygon, const FGteMatrix& Matrix, const std::array<FGteVector, 4>& Corners,
			const std::array<FGteProjection, 4>& Projected, uint8 U, uint8 V, uint8 Width, uint8 Height)
		{
			Polygon.CornerCount = 4;
			Polygon.bTextured = true;
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				const bool bFar = (Corner & 1) != 0;
				const bool bLow = (Corner & 2) != 0;
				Polygon.View[Corner] = ViewPoint(Matrix, Corners[Corner]);
				Polygon.ScreenX[Corner] = Projected[Corner].ScreenX;
				Polygon.ScreenY[Corner] = Projected[Corner].ScreenY;
				Polygon.U[Corner] = static_cast<uint8>(bFar ? U + Width - 1 : U);
				Polygon.V[Corner] = static_cast<uint8>(bLow ? V + Height - 1 : V);
				Polygon.TexCoords[Corner] = {static_cast<float>(bFar ? U + Width : U), static_cast<float>(bLow ? V + Height : V)};
			}
			Polygon.TexelMin = {U, V};
			Polygon.TexelMax = {static_cast<uint8>(U + Width - 1), static_cast<uint8>(V + Height - 1)};
		}

		int32 WrapUnits(int32 Delta, int32 World)
		{
			if (Delta < -HalfWorldLimit)
			{
				Delta += World;
			}
			if (Delta > HalfWorldLimit - 1)
			{
				Delta -= World;
			}
			return Delta;
		}

		int32 Wrap(int32 Value, int32 Count)
		{
			const int32 Rest = Value % Count;
			return Rest < 0 ? Rest + Count : Rest;
		}

		/** What the GTE drops of a point it turns by a matrix (M * V >> 12): each row's last twelve bits, as a part of a unit. */
		std::array<float, 3> DroppedFraction(const FGteMatrix& Matrix, const FGteVector& Point)
		{
			std::array<float, 3> Fraction{};
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				const int64 Turned = int64{Matrix.M[Row][0]} * Point[0] + int64{Matrix.M[Row][1]} * Point[1] + int64{Matrix.M[Row][2]} * Point[2];
				Fraction[Row] = static_cast<float>(Turned & (FGteMatrix::One - 1)) / static_cast<float>(FGteMatrix::One);
			}
			return Fraction;
		}
	}

	void DrawWorldMapShadows(std::span<const FWorldMapShadow> Shadows, const FWorldMapGround& Ground, int16 VehicleHeading, const FWorldMapDrawState& State,
		EWorldMapCulling Culling, FWorldMapFrameBuilder& Builder)
	{
		const std::array<FGteVector, 4> Corners{{{-ShadowHalf, 0, ShadowHalf}, {ShadowHalf, 0, ShadowHalf}, {-ShadowHalf, 0, -ShadowHalf},
			{ShadowHalf, 0, -ShadowHalf}}};
		FGte Gte = WorldMapDrawingGte(State, WorldMapReach(Culling));
		for (const FWorldMapShadow& Shadow : Shadows)
		{
			// The ground's slope under it: across (north cross the normal), the normal, and along (the normal cross across).
			const int32 X = static_cast<int32>(static_cast<uint32>(Shadow.X) << 12);
			const int32 Z = static_cast<int32>(static_cast<uint32>(Shadow.Z) << 12);
			const int32 Y = Ground.Height(X, Z);
			const std::array<int32, 3> Normal = Ground.GroundNormal(X, Z);
			const FGteLongVector Across = VectorNormal(OuterProduct12({0, 0, FGteMatrix::One}, Normal));
			const FGteLongVector Along = VectorNormal(OuterProduct12(Normal, Across));
			FGteMatrix Lying;
			for (std::size_t Column = 0; Column < 3; ++Column)
			{
				Lying.M[0][Column] = static_cast<int16>(Across[Column]);
				Lying.M[1][Column] = static_cast<int16>(Normal[Column]);
				Lying.M[2][Column] = static_cast<int16>(Along[Column]);
			}
			if (Shadow.Kind == GearShadow)
			{
				Lying = ScaleColumns(Lying, {GearShadowScale, GearShadowScale, GearShadowScale});
			}
			else if (Shadow.Kind == VehicleShadow)
			{
				Lying = ScaleColumns(MulMatrix(Lying, RotationY(VehicleHeading)), VehicleShadowScale);
			}
			// Its place relative to the camera's target, unwrapped.
			const FGteVector Place{static_cast<int16>((X - State.Target.X) >> 12), static_cast<int16>(Y >> 12), static_cast<int16>((State.Target.Z - Z) >> 12)};
			Gte.Transform.M = MulMatrix(State.Camera, Lying).M;
			Gte.Transform.T = RotTrans(State.Camera, Place);
			std::array<FGteProjection, 4> Projected{};
			uint32 Flag = 0;
			for (std::size_t Corner = 0; Corner < 3; ++Corner)
			{
				Projected[Corner] = Gte.Rtps(Corners[Corner]);
				Flag |= Projected[Corner].Flag;
			}
			if (Culling == EWorldMapCulling::Original && (Flag & GteFlag::Error) != 0)
			{
				continue;
			}
			Projected[3] = Gte.Rtps(Corners[3]);
			uint16 Nearest = Projected[0].ScreenZ;
			for (const FGteProjection& Corner : Projected)
			{
				Nearest = std::min(Nearest, Corner.ScreenZ);
			}
			if (Nearest >= ShadowReach * WorldMapReach(Culling) + State.EyeBack)
			{
				continue;
			}
			Builder.Anchor(EWorldMapAnchorKind::Shadow, static_cast<uint32>(&Shadow - Shadows.data()) << 2 | static_cast<uint32>(Shadow.Kind & 3), Gte.Transform);
			FWorldMapPolygon& Polygon = Builder.Add(static_cast<uint16>(WorldMapSortDepth(Nearest, State) >> BucketShift));
			Polygon.Layer = EWorldMapLayer::Shadow;
			SetTexturedQuad(Polygon, Gte.Transform, Corners, Projected, ShadowU, ShadowV, ShadowSide, ShadowSide);
			Polygon.bSemiTransparent = true;
			Polygon.TexturePage = ShadowPage;
			Polygon.Clut = ShadowClut;
			Polygon.Colors.fill(ShadowColor);
		}
	}

	void DrawWorldMapBillboards(const FWorldMapScene& Scene, const FWorldMapTerrain& Terrain, const FWorldMapBillboardArea& Area, int16 Roll,
		const FWorldMapDrawState& State, EWorldMapCulling Culling, bool bInDepthOrder, FWorldMapFrameBuilder& Builder)
	{
		const int32 Reach = WorldMapReach(Culling);
		FWorldMapBillboardArea Current;
		const FWorldMapBillboardArea* Drawn = &Area;
		if (Culling != EWorldMapCulling::Original || Area.BlockStates.size() != static_cast<std::size_t>(Area.Across * Area.Across))
		{
			// Every block about the camera's this frame, within the reach (and after a change of culling).
			Current.BlockX = State.Target.X >> 23;
			Current.BlockZ = State.Target.Z >> 23;
			Current.Across = WorldMapBlocksAcross(Culling);
			Current.BlockStates.assign(static_cast<std::size_t>(Current.Across * Current.Across), 1);
			Drawn = &Current;
		}
		FGte Gte = WorldMapDrawingGte(State, Reach);
		Gte.Transform = RotationZ(-Roll);
		const int32 CameraX = State.Target.X >> 12;
		const int32 CameraZ = State.Target.Z >> 12;
		const int32 WorldX = Terrain.BlocksX() * BlockUnits;
		const int32 WorldZ = Terrain.BlocksZ() * BlockUnits;
		const int32 Before = Drawn->Across / 2;
		const int32 Limit = Culling == EWorldMapCulling::Original ? BillboardLimit : std::numeric_limits<int32>::max();
		const std::size_t First = Builder.LinkedCount();
		int32 Count = 0;
		for (int32 Row = 0; Row < Drawn->Across && Count < Limit; ++Row)
		{
			for (int32 Column = 0; Column < Drawn->Across && Count < Limit; ++Column)
			{
				if (Drawn->BlockStates[static_cast<std::size_t>(Row * Drawn->Across + Column)] == Outside)
				{
					continue;
				}
				const int32 Block = Wrap(Drawn->BlockZ - Before + Row, Terrain.BlocksZ()) * Terrain.BlocksX() + Wrap(Drawn->BlockX - Before + Column, Terrain.BlocksX());
				const std::vector<FWorldMapBillboard>& Billboards = Scene.Billboards[static_cast<std::size_t>(Block)];
				for (const FWorldMapBillboard& Billboard : Billboards)
				{
					if (Count >= Limit)
					{
						break;
					}
					// Its foot through the camera's rotation and translation; its quad then only turned with the roll.
					const int32 X = WrapUnits(Billboard.X - CameraX, WorldX);
					const int32 Z = WrapUnits(Billboard.Z - CameraZ, WorldZ);
					const FGteVector Place{static_cast<int16>(X), Billboard.Y, static_cast<int16>(-Z)};
					const FGteLongVector Foot = ApplyMatrix(State.Camera, Place);
					Gte.Transform.T = {Foot[0] + State.Camera.T[0], Foot[1] + State.Camera.T[1], Foot[2] + State.Camera.T[2]};
					std::array<FGteProjection, 4> Projected{};
					uint32 Flag = 0;
					for (std::size_t Corner = 0; Corner < 3; ++Corner)
					{
						Projected[Corner] = Gte.Rtps(BillboardCorners[Corner]);
						Flag |= Projected[Corner].Flag;
					}
					if (Culling == EWorldMapCulling::Original
						&& ((Flag & GteFlag::Error) != 0 || !IsOnScreen({Projected[0], Projected[1], Projected[2]})))
					{
						continue;
					}
					const uint16 Depth = Projected[2].ScreenZ;
					if (Depth >= BillboardReach * Reach + State.EyeBack)
					{
						continue;
					}
					Projected[3] = Gte.Rtps(BillboardCorners[3]);
					const int32 DepthCue = std::min<int32>(static_cast<uint16>(Projected[3].DepthCue), DepthCueMost);
					// The view stands it where its foot lies to the last bit: whole units would shake each tree by itself as the camera moves.
					const std::array<float, 3> Fraction = DroppedFraction(State.Camera, Place);
					Builder.Anchor(EWorldMapAnchorKind::Billboard, static_cast<uint32>(Block) << 16 | static_cast<uint32>(&Billboard - Billboards.data()), Gte.Transform,
						Fraction);
					FWorldMapPolygon& Polygon = Builder.Add(static_cast<uint16>(WorldMapSortDepth(Depth, State) >> BucketShift));
					Polygon.Layer = EWorldMapLayer::Billboard;
					SetTexturedQuad(Polygon, Gte.Transform, BillboardCorners, Projected, 0, BillboardV, BillboardWidth, BillboardHeight);
					for (std::array<float, 3>& Corner : Polygon.View)
					{
						for (std::size_t Axis = 0; Axis < 3; ++Axis)
						{
							Corner[Axis] += Fraction[Axis];
						}
					}
					Polygon.TexturePage = BillboardPage;
					Polygon.Clut = static_cast<uint16>((ShadeRow + (DepthCue >> ShadeShift)) << 6 | ShadeColumn >> 4);
					Polygon.Colors.fill(FDrawColor{});
					++Count;
				}
			}
		}
		if (bInDepthOrder)
		{
			Builder.LinkNearestFirst(First);
		}
	}
}
