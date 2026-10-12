#pragma once

#include "Foundation/Types.h"
#include "WorldMap/WorldMapDrawing.h"

#include <span>
#include <vector>

namespace Weltall
{
	class FWorldMapGround;
	struct FWorldMapScene;
	struct FWorldMapShadow;

	/**
	 * The blocks the billboards of a frame are drawn from: the original draws them before it culls the
	 * terrain, so from the blocks about the camera's block and the tests of the frame before.
	 */
	struct FWorldMapBillboardArea
	{
		/** The camera's block, x and z. */
		int32 BlockX = 0;
		int32 BlockZ = 0;
		/** The blocks across (and down) the area, and each one's frustum test (-1 outside), row by row. */
		int32 Across = 0;
		std::vector<int32> BlockStates;
	};

	/**
	 * The shadows the tasks asked for this tick, as the program draws them: a 32-unit square on
	 * the ground at each one's place, turned to the ground's slope, half as big again for a gear and
	 * stretched along the Yggdrasil's heading (VehicleHeading) for the vehicle, a dim semi-transparent
	 * texture sorted by its nearest corner.
	 */
	WELTALLCORE_API void DrawWorldMapShadows(std::span<const FWorldMapShadow> Shadows, const FWorldMapGround& Ground, int16 VehicleHeading,
		const FWorldMapDrawState& State, EWorldMapCulling Culling, FWorldMapFrameBuilder& Builder);

	/**
	 * The billboards (trees) as the program draws them: each of the area's blocks' lists (under
	 * the other cullings, every block about the camera's this frame within its reach), every billboard a
	 * 48 x 72 quad standing at its place and facing the screen (turned only with the camera's roll), its
	 * palette the depth's shade of the models' fogged palettes; at most 512 a frame. Linked in their lists'
	 * order as the original links them, or with bInDepthOrder the nearest first, so that of two trees an
	 * ordering-table entry holds the farther is drawn first.
	 */
	WELTALLCORE_API void DrawWorldMapBillboards(const FWorldMapScene& Scene, const FWorldMapTerrain& Terrain, const FWorldMapBillboardArea& Area,
		int16 Roll, const FWorldMapDrawState& State, EWorldMapCulling Culling, bool bInDepthOrder, FWorldMapFrameBuilder& Builder);
}
