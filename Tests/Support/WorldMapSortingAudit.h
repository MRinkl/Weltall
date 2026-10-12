#pragma once

#include "Field/FieldRenderOptions.h"
#include "Graphics/Vram.h"
#include "Support/SortingDrawing.h"
#include "WorldMap/WorldMapFrame.h"

#include <string>

namespace Weltall::Testing
{
	struct FWorldMapSortingAuditOptions
	{
		/** The view's width in the original's screen pixels about its centre (320: the original's screen; 384: 16:9 at its 216 lines). */
		int32 Width = 384;
		/** How many audit pixels across and down each screen pixel is drawn at. */
		int32 Scale = 2;
		/** How much nearer than what the order draws over it a piece must stand to count, in view units. */
		float Margin = 16.0f;
		/** The lines of the screen the frame was projected for: the world map's 216; the arena's 218. */
		int32 Lines = 216;
		/** The colour the screen is cleared to (0xRRGGBB): the world map's black; the arena's far colour. */
		uint32 Clear = 0;
		/**
		 * The drawing knows the frame's floor (the arena's, which draws its bouts in the world map's frame form): under a depth
		 * sorting a shadow is drawn only over what stands no higher above its floor than a gear's broad shadow's height, over
		 * a gear's pixels 16 units.
		 */
		bool bShadowsOnFloors = false;
		/**
		 * The sorting judged as drawing the frame (WorldMapFrameDraw.cpp). The painter's order: as the original draws. Per
		 * pixel: the sprites stand at their entries' depth against everything and the shadows are kept off them, so nothing
		 * against a sprite counts, and the arena's gears are sorted by depth among themselves, as the field's models are;
		 * everything else keeps the order, the effects too. With the scenery sorted too (the default), nothing between two
		 * pieces of the ground, the place models, the trees and the clouds counts either (the party's ship is none of them),
		 * but where the gears stand under the ground (the arena's); and a blend drawn before what covers it always.
		 */
		EFieldSorting Sorting = EFieldSorting::DepthBufferWithScenery;
	};

	/**
	 * Draws a world map frame twice in software as its polygons (the terrain, the place models, the trees, the sprites,
	 * the shadows, the clouds, the particles; the sky and its band behind everything), once in the frame's order and
	 * once by view depth, as the frame's culling left them (a polygon marked one-sided only where it faces the eye),
	 * their texels read from VRAM, and finds the pixels where something farther is drawn over something nearer.
	 */
	FSortingFrameAudit AuditWorldMapSorting(const FWorldMapFrame& Frame, const FVram& Vram, const FWorldMapSortingAuditOptions& Options);

	/** The frame's polygon Number named for a report: what it is, what it belongs to, which polygon, its entry. */
	std::string DescribeWorldMapSortingPiece(const FWorldMapFrame& Frame, uint32 Number, const FSortingPiece& Piece);
}
