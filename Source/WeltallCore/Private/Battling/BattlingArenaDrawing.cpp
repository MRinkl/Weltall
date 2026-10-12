#include "Battling/BattlingDrawing.h"

#include "Battling/BattlingFight.h"
#include "Graphics/Gte.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int32 FogNear = 0x800;
		constexpr int32 FogFar = 0x1800;
		constexpr int32 FogProjection = 0xC0;
		constexpr int32 BucketShift = 4;

		constexpr int32 ArenaCentre = 0x3F80;
		constexpr int32 WallRadius = 0x3F80;
		constexpr int32 WallSegments = 32;
		constexpr int32 WallSegmentAngle = 0x10;
		constexpr int32 WallSectorHalf = 0x100;
		constexpr int16 WallMiddleY = -0x290;
		constexpr int16 WallTopY = -0x520;
		constexpr int32 WallReach = 0x1C00;
		constexpr uint8 WallTexelsAcross = 0x3F;
		constexpr uint8 WallTexelsUp = 0x7F;

		constexpr int32 CellUnits = FBattlingTerrain::CellUnits;
		constexpr int32 CellsAcross = FBattlingTerrain::Size;
		constexpr uint8 NoColumn = 0xFF;
		/** The wedge's reach in cells: its near corners beside the eye, its far arc, and the replay's narrower one. */
		constexpr int32 WedgeNearCells = 10;
		constexpr int32 WedgeFarCells = 0x18;
		constexpr int32 WedgeNearAngle = 0x280;
		constexpr int32 WedgeFarAngle = 0x580;
		constexpr int32 WedgeFarStep = 0x100;
		constexpr int32 WedgeFarEdges = 6;
		constexpr int32 NarrowNearCells = 0x10;
		constexpr int32 NarrowFarCells = 0x20;
		constexpr int32 NarrowNearAngle = 0x100;
		constexpr int32 NarrowFarAngle = 0x78A;
		constexpr int32 NarrowFarStep = 0x75;
		constexpr int32 NarrowFarEdges = 3;
		/** The fog's far depth in cells, and one more for the cell the eye stands in. */
		constexpr int32 ReachCells = 0x19;

		constexpr int32 ShadowBaseSize = 0x90;
		constexpr uint8 ShadowTexels = 0x3F;
		constexpr FDrawColor ShadowColour{0x10, 0x10, 0x10};

		/** The bout's environment mapping, as the arena's task sets it: a corner's texel half its turned normal's x and y, from the picture's middle. */
		constexpr FMdlEnvironmentMapping FightEnvironment{1, 1, 0x40, 0x40};

		constexpr int16 SkyHeight = 0x60;
		/** The table's last entry, drawn before every other. */
		constexpr uint16 FarthestBucket = 0xFFF;
		/** Where the sky's polygon lies in view space, and how far past the screen's sides it reaches (a 32:9 view shows 228 lines' width more on each). */
		constexpr float SkyDepth = 4096.0f;
		constexpr float SkyBeyond = 240.0f;

		constexpr uint8 StateReplay = 4;
		constexpr uint8 StateResult = 8;

		FDrawColor DrawColour(const FGteColor& Colour)
		{
			return {Colour.R, Colour.G, Colour.B};
		}

		FGteColor GteColour(const FDrawColor& Colour)
		{
			return {Colour.R, Colour.G, Colour.B, 0};
		}

		/** A textured polygon's corners: where the view puts them, where the GTE projected them, and their texels. */
		template <std::size_t Count>
		void SetCorners(FWorldMapPolygon& Polygon, const FGteMatrix& Matrix, const std::array<FGteVector, Count>& Vertices,
			const std::array<FGteProjection, Count>& Projected, const std::array<uint16, Count>& Texels)
		{
			Polygon.CornerCount = static_cast<uint8>(Count);
			uint8 LowU = 0xFF;
			uint8 LowV = 0xFF;
			uint8 HighU = 0;
			uint8 HighV = 0;
			for (std::size_t Corner = 0; Corner < Count; ++Corner)
			{
				Polygon.View[Corner] = ViewPoint(Matrix, Vertices[Corner]);
				Polygon.ScreenX[Corner] = Projected[Corner].ScreenX;
				Polygon.ScreenY[Corner] = Projected[Corner].ScreenY;
				Polygon.U[Corner] = static_cast<uint8>(Texels[Corner]);
				Polygon.V[Corner] = static_cast<uint8>(Texels[Corner] >> 8);
				Polygon.TexCoords[Corner] = {static_cast<float>(Polygon.U[Corner]), static_cast<float>(Polygon.V[Corner])};
				LowU = std::min(LowU, Polygon.U[Corner]);
				LowV = std::min(LowV, Polygon.V[Corner]);
				HighU = std::max(HighU, Polygon.U[Corner]);
				HighV = std::max(HighV, Polygon.V[Corner]);
			}
			Polygon.TexelMin = {LowU, LowV};
			Polygon.TexelMax = {HighU, HighV};
		}

		/** The columns of each row of cells the ground is drawn across, as the program's edge rasterizer leaves them. */
		class FTerrainRows
		{
		public:
			explicit FTerrainRows(const FBattlingArenaTables& InTables) : Tables(InTables)
			{
				First.fill(NoColumn);
				End.fill(0);
			}

			/** Every cell of the arena's outline within the fog's reach of the eye's cell: past it the ground is the far colour, as the screen behind it. */
			void TakeAround(int32 EyeX, int32 EyeZ)
			{
				const int32 Column = EyeX >> 8;
				const int32 Middle = EyeZ >> 8;
				for (int32 Row = std::max(Middle - ReachCells, 0); Row < std::min(Middle + ReachCells + 1, CellsAcross); ++Row)
				{
					const std::size_t At = static_cast<std::size_t>(Row);
					const int32 Low = std::max<int32>(Tables.FirstColumn[At], Column - ReachCells);
					const int32 High = std::min<int32>(std::min<int32>(Tables.LastColumn[At] + 1, CellsAcross), Column + ReachCells + 1);
					if (Low < High)
					{
						First[At] = static_cast<uint8>(Low);
						End[At] = static_cast<uint8>(High);
					}
				}
			}

			/** The wedge from the eye's cell: two near corners beside it, an arc of far corners ahead, joined edge by edge. */
			void TakeWedge(int32 EyeX, int32 EyeZ, int32 Angle, bool bNarrow)
			{
				const int32 Column = EyeX >> 8;
				const int32 Row = EyeZ >> 8;
				const int32 NearCells = bNarrow ? NarrowNearCells : WedgeNearCells;
				const int32 FarCells = bNarrow ? NarrowFarCells : WedgeFarCells;
				const int32 NearAngle = bNarrow ? NarrowNearAngle : WedgeNearAngle;
				const auto Corner = [&](int32 At, int32 Cells) {
					return std::array<int32, 2>{Column + (GteSin(At) * Cells >> 12), Row + (GteCos(At) * Cells >> 12)};
				};
				const std::array<int32, 2> NearLeft = Corner(Angle + NearAngle, NearCells);
				std::array<int32, 2> Last = NearLeft;
				int32 FarAngle = Angle + (bNarrow ? NarrowFarAngle : WedgeFarAngle);
				for (int32 Edge = 0; Edge < (bNarrow ? NarrowFarEdges : WedgeFarEdges); ++Edge)
				{
					const std::array<int32, 2> Next = Corner(FarAngle, FarCells);
					RasterEdge(Last, Next);
					Last = Next;
					FarAngle += bNarrow ? NarrowFarStep : WedgeFarStep;
				}
				const std::array<int32, 2> NearRight = Corner(Angle - NearAngle, NearCells);
				RasterEdge(Last, NearRight);
				RasterEdge(NearLeft, NearRight);
			}

			std::array<uint8, FBattlingArenaTables::Rows> First{};
			/** One past the last column; 0 for a row the wedge misses. */
			std::array<uint8, FBattlingArenaTables::Rows> End{};

		private:
			/** An edge's column at each row it crosses widens the row's span, held inside the arena's outline. */
			void RasterEdge(std::array<int32, 2> From, std::array<int32, 2> To)
			{
				if (From[1] == To[1])
				{
					return;
				}
				if (To[1] < From[1])
				{
					std::swap(From, To);
				}
				int32 Fixed = From[0] << 8;
				const int32 Rows = To[1] - From[1];
				for (int32 Row = From[1]; Row < To[1]; ++Row)
				{
					if (Row >= 0)
					{
						if (Row >= CellsAcross)
						{
							return;
						}
						const std::size_t At = static_cast<std::size_t>(Row);
						int32 Column = Fixed >> 8;
						if (Column < First[At])
						{
							Column = std::max<int32>(Column, Tables.FirstColumn[At]);
							First[At] = static_cast<uint8>(Column);
						}
						if (End[At] < Column)
						{
							Column = std::min<int32>(Column, Tables.LastColumn[At] + 1);
							End[At] = static_cast<uint8>(Column);
						}
					}
					Fixed += (To[0] - From[0]) * 0x100 / Rows;
				}
			}

			const FBattlingArenaTables& Tables;
		};

		/** What every part of the arena is drawn with. */
		struct FArenaDraw
		{
			const FBattlingFight& Fight;
			const FBattlingArenaTables& Tables;
			const FBattlingArenaTextures& Textures;
			const FBattlingTerrain& Terrain;
			const FGteArctangents& Arctangents;
			const FBattlingLighting& Lighting;
			EBattlingCulling Culling;
			/** The GTE with the arena's matrix (the world about the camera's target, heights as they are), the fog and the far colour. */
			FGte Gte;
			FWorldMapFrameBuilder& Builder;

			void DrawWall()
			{
				const FBattlingPoint& Eye = Fight.Eye;
				const FBattlingPoint& Target = Fight.Target;
				const int32 Bearing = GteRatan2(Eye[0] - ArenaCentre, Eye[2] - ArenaCentre, Arctangents) & 0xFFF0;
				const bool bWhole = Culling != EBattlingCulling::Original;
				const int32 Segments = bWhole ? 0x1000 / WallSegmentAngle : WallSegments;
				const auto Place = [&](int32 Angle, int16 Y) {
					return FGteVector{static_cast<int16>((GteSin(Angle) * WallRadius >> 12) - (static_cast<int16>(Target[0]) - ArenaCentre)), Y,
						static_cast<int16>((GteCos(Angle) * WallRadius >> 12) - (static_cast<int16>(Target[2]) - ArenaCentre))};
				};
				int32 Angle = Bearing - WallSectorHalf;
				const FGteColor Base = GteColour(Lighting.FogBase);
				const uint8 V = Textures.WallV;
				const auto Texel = [](uint8 U, uint8 AtV) { return static_cast<uint16>(U | AtV << 8); };
				for (int32 Segment = 0; Segment < Segments; ++Segment)
				{
					const int32 Next = Angle + WallSegmentAngle;
					const std::array<FGteVector, 6> Vertices{Place(Angle, 0), Place(Next, 0), Place(Angle, WallMiddleY), Place(Next, WallMiddleY),
						Place(Angle, WallTopY), Place(Next, WallTopY)};
					std::array<FGteProjection, 6> Projected;
					for (std::size_t Corner = 0; Corner < Vertices.size(); ++Corner)
					{
						Projected[Corner] = Gte.Rtps(Vertices[Corner]);
					}
					Angle = Next;
					const int32 Depth = std::max({Projected[0].ScreenZ, Projected[1].ScreenZ, Projected[2].ScreenZ, Projected[5].ScreenZ});
					if (Depth >= WallReach)
					{
						continue;
					}
					if (bWhole && Projected[0].View[2] <= 0 && Projected[1].View[2] <= 0)
					{
						continue;
					}
					const FDrawColor Colour = DrawColour(Gte.DepthCueColor(Base, Projected[2].DepthCue));
					const uint16 Bucket = static_cast<uint16>(Depth >> BucketShift);
					const auto Band = [&](std::size_t First, uint8 LowU, uint8 HighU) {
						FWorldMapPolygon& Polygon = Builder.Add(Bucket);
						Polygon.Layer = EWorldMapLayer::Terrain;
						Polygon.bTextured = true;
						Polygon.TexturePage = Textures.WallPage;
						Polygon.Clut = Textures.WallClut;
						Polygon.Colors.fill(Colour);
						SetCorners<4>(Polygon, Gte.Transform, {Vertices[First], Vertices[First + 1], Vertices[First + 2], Vertices[First + 3]},
							{Projected[First], Projected[First + 1], Projected[First + 2], Projected[First + 3]},
							{Texel(HighU, static_cast<uint8>(V + WallTexelsAcross)), Texel(HighU, V), Texel(LowU, static_cast<uint8>(V + WallTexelsAcross)),
								Texel(LowU, V)});
					};
					Band(0, WallTexelsAcross, WallTexelsUp);
					Band(2, 0, WallTexelsAcross);
				}
			}

			void DrawShadow(const FBattlingFighter& Fighter, uint32 Side)
			{
				if (Fighter.Ground() == EBattlingGround::Water)
				{
					return;
				}
				const int32 Size = (Fighter.Base[1] + Fighter.Top[1]) / 32 + ShadowBaseSize;
				if (Size <= 0)
				{
					return;
				}
				const int32 X = (Fighter.Base[0] + Fighter.Top[0]) / 2;
				const int32 Z = (Fighter.Base[2] + Fighter.Top[2]) / 2;
				if (!FBattlingTerrain::Covers(X, Z))
				{
					return;
				}
				// A square on the ground's plane under the gear's middle, turned to the ground's slope.
				const FBattlingTerrainSample Ground = Terrain.Sample(X, 0, Z, false);
				FGteMatrix OnGround = FGteMatrix::Identity();
				OnGround.M[1] = {Ground.Normal[0], Ground.Normal[1], Ground.Normal[2]};
				OnGround.T = {X - Fight.Target[0], Ground.Height, Z - Fight.Target[2]};
				FGte Shadow = Gte;
				Shadow.Transform = CompMatrix(Gte.Transform, OnGround);
				const int16 Half = static_cast<int16>(Size);
				const int16 Back = static_cast<int16>(-Half);
				const std::array<FGteVector, 4> Vertices{FGteVector{Back, 0, Back}, FGteVector{Back, 0, Half}, FGteVector{Half, 0, Back}, FGteVector{Half, 0, Half}};
				std::array<FGteProjection, 4> Projected;
				for (std::size_t Corner = 0; Corner < Vertices.size(); ++Corner)
				{
					Projected[Corner] = Shadow.Rtps(Vertices[Corner]);
				}
				// The original drops a shadow its screen shows from below; a wider view only one wholly behind the eye.
				const bool bShown = Culling == EBattlingCulling::Original
					? Nclip(Projected[0].ScreenX, Projected[0].ScreenY, Projected[1].ScreenX, Projected[1].ScreenY, Projected[2].ScreenX, Projected[2].ScreenY) >= 0
					: std::any_of(Projected.begin(), Projected.end(), [](const FGteProjection& Corner) { return Corner.View[2] > 0; });
				if (!bShown)
				{
					return;
				}
				const int32 Depth = std::min({Projected[0].ScreenZ, Projected[1].ScreenZ, Projected[2].ScreenZ, Projected[3].ScreenZ});
				Builder.Anchor(EWorldMapAnchorKind::Shadow, Side, Shadow.Transform);
				FWorldMapPolygon& Polygon = Builder.Add(static_cast<uint16>(Depth >> BucketShift));
				Polygon.Layer = EWorldMapLayer::Shadow;
				Polygon.bTextured = true;
				Polygon.bSemiTransparent = true;
				Polygon.TexturePage = Textures.ShadowPage;
				Polygon.Clut = Textures.ShadowClut;
				Polygon.Colors.fill(ShadowColour);
				const uint8 U = Textures.ShadowU;
				const uint8 V = Textures.ShadowV;
				const auto Texel = [](uint8 AtU, uint8 AtV) { return static_cast<uint16>(AtU | AtV << 8); };
				SetCorners<4>(Polygon, Shadow.Transform, Vertices, Projected,
					{Texel(U, V), Texel(static_cast<uint8>(U + ShadowTexels), V), Texel(U, static_cast<uint8>(V + ShadowTexels)),
						Texel(static_cast<uint8>(U + ShadowTexels), static_cast<uint8>(V + ShadowTexels))});
			}

			void DrawTerrain()
			{
				const FBattlingPoint& Eye = Fight.Eye;
				const FBattlingPoint& Target = Fight.Target;
				FTerrainRows Rows(Tables);
				if (Culling == EBattlingCulling::Original)
				{
					const bool bNarrow = Fight.LatchedState == StateReplay && Fight.State != StateResult;
					Rows.TakeWedge(Eye[0], Eye[2], GteRatan2(Eye[0] - Target[0], Eye[2] - Target[2], Arctangents), bNarrow);
				}
				else
				{
					Rows.TakeAround(Eye[0], Eye[2]);
				}
				const FGteColor Base = GteColour(Lighting.FogBase);
				const int32 LastCell = CellsAcross * CellsAcross - 1;
				for (int32 Row = 0; Row < CellsAcross - 1; ++Row)
				{
					const int32 First = Rows.First[static_cast<std::size_t>(Row)];
					const int32 End = Rows.End[static_cast<std::size_t>(Row)];
					if (End == 0 || First == NoColumn || First >= End)
					{
						continue;
					}
					const int16 Z = static_cast<int16>(Row * CellUnits - Target[2]);
					const int16 FarZ = static_cast<int16>(Z + CellUnits);
					for (int32 Column = First; Column < End; ++Column)
					{
						const int32 At = Row * CellsAcross + Column;
						const auto Height = [&](int32 Offset) { return static_cast<int16>(Terrain.CellAt(std::min(At + Offset, LastCell))); };
						const int16 X = static_cast<int16>(Column * CellUnits - Target[0]);
						const int16 FarX = static_cast<int16>(X + CellUnits);
						const FGteVector Origin{X, Height(0), Z};
						const FGteVector Far{FarX, Height(CellsAcross + 1), FarZ};
						const FGteVector Across{FarX, Height(1), Z};
						const FGteVector Down{X, Height(CellsAcross), FarZ};
						const uint32 Bits = Terrain.CellAt(At) >> 16;
						const uint16 Corner = static_cast<uint16>(Bits & 0xF0F0);
						const std::array<uint16, 4>& Texels = Tables.CellTexels[Bits & 3];
						const std::size_t Page = Bits >> 2 & 3;
						const FGteProjection AtOrigin = Gte.Rtps(Origin);
						const FGteProjection AtFar = Gte.Rtps(Far);
						const auto Triangle = [&](const FGteVector& Second, const FGteVector& Third, const FGteProjection& AtSecond, const FGteProjection& AtThird,
												  uint16 SecondTexel, uint16 ThirdTexel) {
							if (Culling != EBattlingCulling::Original && AtOrigin.View[2] <= 0 && AtSecond.View[2] <= 0 && AtThird.View[2] <= 0)
							{
								return;
							}
							// The original drops a triangle that faces away on its screen; a wider view leaves that to the display, which
							// knows where it draws the corners (told from the GTE's whole units, a triangle barely facing the eye at a
							// crest would be dropped and leave a hairline).
							if (Culling == EBattlingCulling::Original
								&& Nclip(AtOrigin.ScreenX, AtOrigin.ScreenY, AtSecond.ScreenX, AtSecond.ScreenY, AtThird.ScreenX, AtThird.ScreenY) <= 0)
							{
								return;
							}
							const int32 Depth = std::max({AtOrigin.ScreenZ, AtSecond.ScreenZ, AtThird.ScreenZ});
							FWorldMapPolygon& Polygon = Builder.Add(static_cast<uint16>(Depth >> BucketShift));
							Polygon.Layer = EWorldMapLayer::Terrain;
							Polygon.bOneSided = Culling != EBattlingCulling::Original;
							Polygon.bTextured = true;
							Polygon.TexturePage = Textures.TerrainPages[Page];
							Polygon.Clut = Textures.TerrainCluts[Page];
							// The fade by the depth the GTE left for the last corner.
							Polygon.Colors.fill(DrawColour(Gte.DepthCueColor(Base, AtThird.DepthCue)));
							SetCorners<3>(Polygon, Gte.Transform, {Origin, Second, Third}, {AtOrigin, AtSecond, AtThird},
								{static_cast<uint16>(Corner | Texels[0]), static_cast<uint16>(Corner | SecondTexel), static_cast<uint16>(Corner | ThirdTexel)});
						};
						Triangle(Far, Across, AtFar, Gte.Rtps(Across), Texels[3], Texels[1]);
						Triangle(Down, Far, Gte.Rtps(Down), AtFar, Texels[2], Texels[3]);
					}
				}
			}
		};
	}

	FBattlingDrawing::FBattlingDrawing(const FBattlingArenaTables& InTables, const FBattlingArenaTextures& InTextures, const FBattlingTerrain& InTerrain,
		const FGteArctangents& InArctangents)
		: Tables(InTables), Textures(InTextures), Terrain(InTerrain), Arctangents(InArctangents), Hud(InTables, InTextures),
		  SpriteSets(MakeBattlingSpriteSets(InTables, InTextures))
	{
	}

	FGte FBattlingDrawing::BeginScene(const FBattlingFight& Fight, FBattlingFrame& Frame)
	{
		Frame.Pass = Fight.Pass;
		Frame.Columns = FBattlingFrame::ScreenWidth;
		Frame.PictureBlurs = 0;
		Frame.bTrails = false;
		Frame.Screen.clear();
		Frame.TextButtons.clear();
		const FGte View = Fight.View();
		FWorldMapFrame& Scene = Frame.Scene;
		Scene.Tick = Fight.Pass;
		Scene.ScreenCenterX = View.OffsetX >> 16;
		Scene.ScreenCenterY = View.OffsetY >> 16;
		Scene.ProjectionDistance = View.ProjectionDistance;
		Scene.Camera = View.Transform;
		Scene.bGearsUnderGround = true;
		// The frame's form counts z the other way: a display that draws between two frames moves the arena by the target's move.
		Scene.TargetX = Fight.Target[0];
		Scene.TargetZ = -Fight.Target[2];
		Scene.Yaw = 0;
		Scene.WorldUnitsX = 0;
		Scene.WorldUnitsZ = 0;
		Builder.Begin(Scene);
		return View;
	}

	void FBattlingDrawing::DrawGears(const FBattlingFight& Fight, const FGte& View)
	{
		if (!bGears)
		{
			return;
		}
		for (uint32 Side = 0; Side < 2; ++Side)
		{
			const FBattlingPoint& Position = Fight.Fighters[Side].Position;
			FGearView Gear;
			Gear.Gte = View;
			Gear.RootPlace = {Position[0] - Fight.Target[0], Position[1] - Fight.Target[1], Position[2] - Fight.Target[2]};
			Gear.Lights = GearLights(Fight, Side);
			Gear.Environment = FightEnvironment;
			DrawGear(Fight, Side, Gear);
			LightsLeft = Gear.Lights;
		}
	}

	void FBattlingDrawing::DrawDemonstration(const FBattlingFight& Fight, FBattlingFrame& Frame)
	{
		const FGte View = BeginScene(Fight, Frame);
		Frame.bTrails = true;
		Frame.Clear = {0, 0, 0};
		DrawGears(Fight, View);
		FGteMatrix Heights = FGteMatrix::Identity();
		Heights.T = {0, -Fight.Target[1], 0};
		Builder.Anchor(EWorldMapAnchorKind::World, BattlingAnchors::Effects, CompMatrix(View.Transform, Heights));
		DrawEffects(Fight, View, true);
		Builder.Finish();
	}

	void FBattlingDrawing::Draw(const FBattlingFight& Fight, FBattlingFrame& Frame)
	{
		const FGte View = BeginScene(Fight, Frame);
		DrawScreen(Fight, Frame.Screen, &Frame.TextButtons);
		const FBattlingLighting& Lighting = Tables.Lighting[std::min<std::size_t>(Fight.Lighting, Tables.Lighting.size() - 1)];
		FWorldMapFrame& Scene = Frame.Scene;
		Frame.Clear = Lighting.Far;

		// The sky: from its colour at the top to the far colour at the horizon's line.
		Builder.Anchor(EWorldMapAnchorKind::Sky, BattlingAnchors::Sky, FGteMatrix::Identity());
		FWorldMapPolygon& Sky = Builder.Add(FarthestBucket);
		Sky.Layer = EWorldMapLayer::Sky;
		Sky.CornerCount = 4;
		Sky.bGouraud = true;
		Sky.Colors = {Lighting.SkyTop, Lighting.SkyTop, Lighting.Far, Lighting.Far};
		Sky.ScreenX = {0, static_cast<int16>(FBattlingFrame::ScreenWidth), 0, static_cast<int16>(FBattlingFrame::ScreenWidth)};
		Sky.ScreenY = {0, 0, SkyHeight, SkyHeight};
		const float PerPixel = SkyDepth / static_cast<float>(std::max<int32>(View.ProjectionDistance, 1));
		for (std::size_t Corner = 0; Corner < 4; ++Corner)
		{
			const float Across = (Corner & 1) != 0 ? static_cast<float>(FBattlingFrame::ScreenWidth) + SkyBeyond : -SkyBeyond;
			Sky.View[Corner] = {(Across - static_cast<float>(Scene.ScreenCenterX)) * PerPixel,
				(static_cast<float>(Sky.ScreenY[Corner]) - static_cast<float>(Scene.ScreenCenterY)) * PerPixel, SkyDepth};
		}

		// The arena lies about the camera's target across the ground, at its own heights.
		FGteMatrix Heights = FGteMatrix::Identity();
		Heights.T = {0, -Fight.Target[1], 0};
		FArenaDraw Arena{Fight, Tables, Textures, Terrain, Arctangents, Lighting, Culling, View, Builder};
		Arena.Gte.Transform = CompMatrix(View.Transform, Heights);
		Arena.Gte.FarColor = {Lighting.Far.R << 4, Lighting.Far.G << 4, Lighting.Far.B << 4};
		Arena.Gte.SetFogNearFar(FogNear, FogFar, FogProjection);
		DrawGears(Fight, View);
		// The effects move between frames as the arena does.
		Builder.Anchor(EWorldMapAnchorKind::World, BattlingAnchors::Effects, Arena.Gte.Transform);
		DrawEffects(Fight, View);
		GearAmbient = Lighting.Back;
		bHitLightsHalved = Lighting.bHalvesLights;
		Builder.Anchor(EWorldMapAnchorKind::World, BattlingAnchors::Wall, Arena.Gte.Transform);
		Arena.DrawWall();
		Arena.DrawShadow(Fight.Fighters[0], 0);
		Arena.DrawShadow(Fight.Fighters[1], 1);
		Builder.Anchor(EWorldMapAnchorKind::World, BattlingAnchors::Ground, Arena.Gte.Transform);
		Arena.DrawTerrain();
		Builder.Finish();
	}
}
