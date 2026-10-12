#include "WorldMap/WorldMapDrawing.h"

#include "Graphics/GteVectors.h"
#include "Graphics/SpriteDrawing.h"
#include "WorldMap/WorldMapContext.h"
#include "WorldMap/WorldMapDrawingTables.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr uint16 CloudScreenWidth = 320;
		constexpr uint16 CloudScreenHeight = 216;
		constexpr int32 CloudBucketShift = 4;

		/** The clouds are kept within four tiles of 2048 units each way, and drawn from the copy nearest the camera's target. */
		constexpr int32 CloudSpanMask = 0x1FFFFFF;
		constexpr int32 CloudHalfSpan = 0x1000;
		constexpr int32 CloudSpanUnits = 0x2000;
		/** Under the original's culling a cloud is drawn only inside a wedge this far either side of the camera's yaw, behind the eye. */
		constexpr int32 CloudWedge = 0x169;
		/** Under the original's culling a frame starts no cloud past this many quads (the last one drawn may add up to 48). */
		constexpr int32 CloudQuadLimit = 0xF0;
		/** The clouds lean toward the camera by an eighth of its pitch past the vertical. */
		constexpr int32 CloudLeanStart = 0x400;
		constexpr int32 CloudLeanDivisor = 8;
		/** A cloud's origin nearer than these depths is drawn in 16 or 4 pieces a layer, else whole; a whole one ends past these. */
		constexpr uint16 CloudNear = 0x400;
		constexpr uint16 CloudMiddle = 0x580;
		constexpr uint16 CloudFarthest = 0xD00;
		constexpr uint16 CloudOneLayer = 0xB00;
		constexpr int32 CloudLayers = 3;
		/** The near pieces: 4 x 4 squares of 96 units over the whole cloud's 384, from its first corner, the layers 8 units apart. */
		constexpr int32 CloudPieces = 4;
		constexpr int16 CloudPieceSide = 0x60;
		constexpr int16 CloudSide = 0x180;
		constexpr int16 CloudLayerRise = 8;
		/** The texels a piece covers, less one: a whole cloud's 64, a middle piece's 32, a near one's 16. */
		constexpr uint16 FarTexels = 0x3F;
		constexpr uint16 MiddleTexels = 0x1F;
		constexpr uint16 NearTexels = 0xF;
		/** The GTE's errors that drop a cloud and its near pieces (all but the division's overflow), and those that drop the rest. */
		constexpr uint32 CloudErrors = 0x7F85E000;
		constexpr uint32 PieceErrors = GteFlag::Error;
		/** As the scene's start presets the clouds' packets: a dim colour added to the screen, from the 4-bit page at (3C0h, 100h). */
		constexpr FDrawColor CloudColor{0x26, 0x26, 0x26};
		constexpr uint16 CloudPage = 0x3F;
		constexpr uint16 CloudClut = 0x7F93;

		using FCloudCorners = std::array<FGteVector, 4>;
		using FCloudProjection = std::array<FGteProjection, 4>;

		/** The clouds' quads as the routine draws them, through the matrix of the cloud at hand, and how many the frame has. */
		struct FCloudDrawing
		{
			bool bOriginal = true;
			FWorldMapFrameBuilder& Builder;
			FGte Gte;
			const FWorldMapDrawState& State;
			int32 Quads = 0;

			/** The corners projected; the program drops the quad on an error of the first three or of the fourth. */
			bool Project(const FCloudCorners& Corners, uint32 Errors, FCloudProjection& Projected) const
			{
				uint32 Flag = 0;
				for (std::size_t Corner = 0; Corner < Corners.size(); ++Corner)
				{
					Projected[Corner] = Gte.Rtps(Corners[Corner]);
					Flag |= Projected[Corner].Flag;
				}
				return (Flag & Errors) == 0;
			}

			/**
			 * The depth a quad is linked at, or -1 where it is dropped: on an error, or under the original's culling when no
			 * corner lies on the screen across or none down. Its farthest corner as the program finds it, which misses the
			 * fourth when the third is farther than the first two.
			 */
			int32 Depth(const FCloudCorners& Corners, uint32 Errors, FCloudProjection& Projected) const
			{
				if (!Project(Corners, Errors, Projected))
				{
					return -1;
				}
				if (bOriginal)
				{
					bool bAcross = false;
					bool bDown = false;
					for (const FGteProjection& Corner : Projected)
					{
						bAcross = bAcross || static_cast<uint16>(Corner.ScreenX) < CloudScreenWidth;
						bDown = bDown || static_cast<uint16>(Corner.ScreenY) < CloudScreenHeight;
					}
					if (!bAcross || !bDown)
					{
						return -1;
					}
				}
				const int32 Nearer = std::max(Projected[0].ScreenZ, Projected[1].ScreenZ);
				if (Nearer < Projected[2].ScreenZ)
				{
					return Projected[2].ScreenZ;
				}
				return std::max<int32>(Nearer, Projected[3].ScreenZ);
			}

			void Link(const FCloudCorners& Corners, const FCloudProjection& Projected, int32 Depth, uint16 Texel, uint16 Texels)
			{
				FWorldMapPolygon& Polygon = Builder.Add(static_cast<uint16>(WorldMapSortDepth(Depth, State) >> CloudBucketShift));
				Polygon.Layer = EWorldMapLayer::Cloud;
				Polygon.CornerCount = 4;
				Polygon.bTextured = true;
				Polygon.bSemiTransparent = true;
				Polygon.TexturePage = CloudPage;
				Polygon.Clut = CloudClut;
				const uint8 U = static_cast<uint8>(Texel);
				const uint8 V = static_cast<uint8>(Texel >> 8);
				for (std::size_t Corner = 0; Corner < Corners.size(); ++Corner)
				{
					Polygon.View[Corner] = ViewPoint(Gte.Transform, Corners[Corner]);
					Polygon.ScreenX[Corner] = Projected[Corner].ScreenX;
					Polygon.ScreenY[Corner] = Projected[Corner].ScreenY;
					Polygon.Colors[Corner] = CloudColor;
					Polygon.U[Corner] = static_cast<uint8>((Corner & 1) != 0 ? U | Texels : U);
					Polygon.V[Corner] = static_cast<uint8>((Corner & 2) != 0 ? V | Texels : V);
					Polygon.TexCoords[Corner] = {static_cast<float>(Polygon.U[Corner]), static_cast<float>(Polygon.V[Corner])};
				}
				Polygon.TexelMin = {U, V};
				Polygon.TexelMax = {static_cast<uint8>(U | Texels), static_cast<uint8>(V | Texels)};
				++Quads;
			}

			void Draw(const FCloudCorners& Corners, uint32 Errors, uint16 Texel, uint16 Texels)
			{
				FCloudProjection Projected{};
				const int32 At = Depth(Corners, Errors, Projected);
				if (At >= 0)
				{
					Link(Corners, Projected, At, Texel, Texels);
				}
			}
		};

		FCloudCorners CornersAt(std::span<const FGteVector> Table, std::size_t First)
		{
			return {Table[First], Table[First + 1], Table[First + 2], Table[First + 3]};
		}

		int32 WrapCloudOffset(int32 Offset)
		{
			if (Offset < -CloudHalfSpan)
			{
				return Offset + CloudSpanUnits;
			}
			return Offset >= CloudHalfSpan ? Offset - CloudSpanUnits : Offset;
		}
	}

	void DrawWorldMapClouds(std::span<const FWorldMapCloud> Clouds, const FWorldMapDrawingTables& Tables, const FWorldMapDrawState& State,
		EWorldMapCulling Culling, FWorldMapFrameBuilder& Builder)
	{
		const bool bOriginal = Culling == EWorldMapCulling::Original;
		const int32 Reach = WorldMapReach(Culling);
		const FGteMatrix Lean = MulMatrix(RotationY(State.Yaw), MulMatrix(RotationX((State.Pitch + CloudLeanStart) / CloudLeanDivisor), RotationY(-State.Yaw)));
		const FGteMatrix Turn = MulMatrix(State.Camera, Lean);
		// The wedge's apex: twice the eye's offset from the target, and half a unit further back.
		const int32 ApexX = State.Eye[0] * 2 + (-GteSin(State.Yaw) >> 1);
		const int32 ApexZ = State.Eye[2] * 2 + (-GteCos(State.Yaw) >> 1);
		const std::array<int16, 2> LeftEdge{static_cast<int16>(GteSin(State.Yaw - CloudWedge)), static_cast<int16>(GteCos(State.Yaw - CloudWedge))};
		const std::array<int16, 2> RightEdge{static_cast<int16>(GteSin(State.Yaw + CloudWedge)), static_cast<int16>(GteCos(State.Yaw + CloudWedge))};
		const int32 TargetX = State.Target.X & CloudSpanMask;
		const int32 TargetZ = State.Target.Z & CloudSpanMask;

		FCloudDrawing Drawing{bOriginal, Builder, WorldMapDrawingGte(State), State};
		const std::span<const FGteVector> Whole(Tables.CloudCorners);
		const std::span<const FGteVector> Middle(Tables.CloudMiddleCorners);
		for (const FWorldMapCloud& Cloud : Clouds)
		{
			if (bOriginal && Drawing.Quads > CloudQuadLimit)
			{
				return;
			}
			const int32 X = WrapCloudOffset((Cloud.Position.X - TargetX) >> 12);
			const int32 Z = -WrapCloudOffset((Cloud.Position.Z - TargetZ) >> 12);
			if (bOriginal)
			{
				const int16 FromApexX = static_cast<int16>(X - ApexX);
				const int16 FromApexZ = static_cast<int16>(Z - ApexZ);
				if (Nclip(FromApexX, FromApexZ, RightEdge[0], RightEdge[1], 0, 0) > 0 || Nclip(0, 0, LeftEdge[0], LeftEdge[1], FromApexX, FromApexZ) > 0)
				{
					continue;
				}
			}
			const FGteLongVector Place = RotTrans(State.Camera, {static_cast<int16>(X), static_cast<int16>(Cloud.Position.Y >> 12), static_cast<int16>(Z)});
			Drawing.Gte.Transform.M = Turn.M;
			Drawing.Gte.Transform.T = {Place[0], Place[1], Place[2]};
			const FGteProjection Origin = Drawing.Gte.Rtps({0, 0, 0});
			if ((Origin.Flag & CloudErrors) != 0)
			{
				continue;
			}
			Builder.Anchor(EWorldMapAnchorKind::Cloud, static_cast<uint32>(&Cloud - Clouds.data()), Drawing.Gte.Transform);
			const std::size_t Look = static_cast<std::size_t>(Cloud.Look) << 2;
			if (Origin.ScreenZ > CloudMiddle)
			{
				for (int32 Layer = 0; Layer < CloudLayers; ++Layer)
				{
					const FCloudCorners Corners = CornersAt(Whole, static_cast<std::size_t>(Layer) * 4);
					FCloudProjection Projected{};
					const int32 Depth = Drawing.Depth(Corners, PieceErrors, Projected);
					if (Depth < 0)
					{
						continue;
					}
					if (Depth > CloudFarthest * Reach + State.EyeBack)
					{
						break;
					}
					Drawing.Link(Corners, Projected, Depth, Tables.CloudTexels[Look + static_cast<std::size_t>(Layer)], FarTexels);
					if (Depth > CloudOneLayer * Reach + State.EyeBack)
					{
						break;
					}
				}
			}
			else if (Origin.ScreenZ > CloudNear)
			{
				for (int32 Layer = 0; Layer < CloudLayers; ++Layer)
				{
					for (int32 Piece = 0; Piece < 4; ++Piece)
					{
						const uint16 Texel = static_cast<uint16>(Tables.CloudTexels[Look + static_cast<std::size_t>(Layer)] + ((Piece & 2) << 12) + ((Piece & 1) << 5));
						Drawing.Draw(CornersAt(Middle, static_cast<std::size_t>(Layer * 4 + Piece) * 4), PieceErrors, Texel, MiddleTexels);
					}
				}
			}
			else
			{
				const FGteVector First = Whole[0];
				for (int32 Layer = 0; Layer < CloudLayers; ++Layer)
				{
					const int16 Y = static_cast<int16>(First[1] - CloudLayerRise * Layer);
					const int16 FarX = static_cast<int16>(First[0] + CloudSide);
					const int16 FarZ = static_cast<int16>(First[2] - CloudSide);
					// The whole layer must project before its pieces are drawn.
					FCloudProjection Projected{};
					if (!Drawing.Project({{{First[0], Y, First[2]}, {FarX, Y, First[2]}, {First[0], Y, FarZ}, {FarX, Y, FarZ}}}, CloudErrors, Projected))
					{
						continue;
					}
					for (int32 Row = 0; Row < CloudPieces; ++Row)
					{
						for (int32 Column = 0; Column < CloudPieces; ++Column)
						{
							const int16 Left = static_cast<int16>(First[0] + CloudPieceSide * Column);
							const int16 Top = static_cast<int16>(First[2] - CloudPieceSide * Row);
							const int16 Right = static_cast<int16>(Left + CloudPieceSide);
							const int16 Bottom = static_cast<int16>(Top - CloudPieceSide);
							const uint16 Texel = static_cast<uint16>(Tables.CloudTexels[Look + static_cast<std::size_t>(Layer)] + (Row << 12) + (Column << 4));
							Drawing.Draw({{{Left, Y, Top}, {Right, Y, Top}, {Left, Y, Bottom}, {Right, Y, Bottom}}}, CloudErrors, Texel, NearTexels);
						}
					}
				}
			}
		}
	}
}
