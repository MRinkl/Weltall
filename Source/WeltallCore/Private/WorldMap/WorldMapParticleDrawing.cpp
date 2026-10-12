#include "WorldMap/WorldMapDrawing.h"

#include "Graphics/GteVectors.h"
#include "Graphics/SpriteDrawing.h"
#include "WorldMap/WorldMapDrawingTables.h"
#include "WorldMap/WorldMapParticles.h"
#include "WorldMap/WorldMapTerrain.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int16 ParticleScreenWidth = 320;
		constexpr int16 ParticleScreenHeight = 216;
		constexpr int32 ParticleBucketShift = 4;
		/** A particle is drawn while its last corner is nearer than this. */
		constexpr uint16 ParticleReach = 0xC00;
		/** The colour step's flag that turns a particle by its angle. */
		constexpr uint32 TurnedFlag = 0x1000000;
		/** As the scene's start presets the particles' packets: semi-transparent, from the palette at (100h, 1FFh). */
		constexpr uint16 ParticleClut = 0x7FD0;

		static_assert(std::tuple_size_v<decltype(FWorldMapDrawingTables::ParticleCorners)> == FWorldMapParticles::ShapeCount);
	}

	void DrawWorldMapParticles(const FWorldMapParticles& Particles, const FWorldMapDrawingTables& Tables, const FWorldMapGround& Ground,
		const FWorldMapDrawState& State, EWorldMapCulling Culling, FWorldMapFrameBuilder& Builder)
	{
		const bool bOriginal = Culling == EWorldMapCulling::Original;
		FGte Gte = WorldMapDrawingGte(State);
		const std::span<const FWorldMapParticle> All = Particles.Particles();
		for (std::size_t Number = 0; Number < All.size(); ++Number)
		{
			const FWorldMapParticle& Particle = All[Number];
			if (Particle.Shape == 0)
			{
				continue;
			}
			const std::size_t Shape = static_cast<std::size_t>(Particle.Shape);
			// A particle faces the screen: its own scale and turn about the view's axis, placed where the camera sees its place.
			FGteMatrix Facing = (Particle.ColourStep & TurnedFlag) != 0 ? RotationZ(Particle.Angle) : FGteMatrix::Identity();
			Facing = ScaleColumns(Facing, {static_cast<uint16>(Particle.Size[0]), static_cast<uint16>(Particle.Size[1]), FGteMatrix::One});
			int32 X = (Particle.Position[0] >> 12) - (State.Target.X >> 12);
			int32 Z = (Particle.Position[2] >> 12) - (State.Target.Z >> 12);
			Ground.WrapDeltaUnits(X, Z);
			const FGteLongVector Place = ApplyMatrix(State.Camera, {static_cast<int16>(X), static_cast<int16>(Particle.Position[1] >> 12), static_cast<int16>(-Z)});
			Facing.T = {Place[0] + State.Camera.T[0], Place[1] + State.Camera.T[1], Place[2] + State.Camera.T[2]};
			Gte.Transform = Facing;

			const std::array<FGteVector, 4>& Corners = Tables.ParticleCorners[Shape];
			std::array<FGteProjection, 4> Projected{};
			uint32 Flag = 0;
			for (std::size_t Corner = 0; Corner < Corners.size(); ++Corner)
			{
				Projected[Corner] = Gte.Rtps(Corners[Corner]);
				Flag |= Projected[Corner].Flag;
			}
			if ((Flag & GteFlag::Error) != 0)
			{
				continue;
			}
			if (bOriginal)
			{
				bool bAcross = false;
				bool bDown = false;
				for (const FGteProjection& Corner : Projected)
				{
					bAcross = bAcross || Corner.ScreenX < ParticleScreenWidth;
					bDown = bDown || Corner.ScreenY < ParticleScreenHeight;
				}
				if (!bAcross || !bDown)
				{
					continue;
				}
			}
			const uint16 Depth = Projected[3].ScreenZ;
			if (Depth >= ParticleReach * WorldMapReach(Culling) + State.EyeBack)
			{
				continue;
			}
			// Its life falls by one a tick: with the tick added it tells the particle from a later one in its slot.
			Builder.Anchor(EWorldMapAnchorKind::Particle, static_cast<uint32>(Number) << 16 | ((static_cast<uint32>(Particle.Life) + State.Tick) & 0xFFFF), Facing);
			FWorldMapPolygon& Polygon = Builder.Add(static_cast<uint16>(WorldMapSortDepth(Depth, State) >> ParticleBucketShift));
			Polygon.Layer = EWorldMapLayer::Particle;
			Polygon.CornerCount = 4;
			Polygon.bTextured = true;
			Polygon.bSemiTransparent = true;
			Polygon.TexturePage = Particle.TexturePage;
			Polygon.Clut = ParticleClut;
			const FDrawColor Color{static_cast<uint8>(Particle.Colour), static_cast<uint8>(Particle.Colour >> 8), static_cast<uint8>(Particle.Colour >> 16)};
			const std::array<uint16, 4>& Texels = Tables.ParticleTexels[Shape];
			uint8 LowU = 0xFF;
			uint8 LowV = 0xFF;
			uint8 HighU = 0;
			uint8 HighV = 0;
			for (std::size_t Corner = 0; Corner < Corners.size(); ++Corner)
			{
				Polygon.View[Corner] = ViewPoint(Facing, Corners[Corner]);
				Polygon.ScreenX[Corner] = Projected[Corner].ScreenX;
				Polygon.ScreenY[Corner] = Projected[Corner].ScreenY;
				Polygon.Colors[Corner] = Color;
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
	}
}
