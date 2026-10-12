#pragma once

#include "Field/FieldFrame.h"
#include "Field/FieldRenderOptions.h"
#include "Field/FieldScene.h"
#include "Graphics/Vram.h"
#include "Support/SortingDrawing.h"

#include <string>

namespace Weltall::Testing
{
	struct FSortingAuditOptions
	{
		/** The view's width in the original's screen pixels about its centre (320: the original's screen; 398: 16:9). */
		int32 Width = 398;
		/** How many audit pixels across and down each screen pixel is drawn at. */
		int32 Scale = 2;
		/** How much nearer than what the order draws over it a piece must stand to count, in view units. */
		float Margin = 16.0f;
		/**
		 * The sorting judged as drawing the frame. The painter's order: as the original draws. The port's per-pixel
		 * sorting (PortSorting): the characters' sprites are sorted by depth against everything, and the 3D models (the
		 * gears, the children's models) against everything but the shadows, so nothing against a sprite counts, nor a
		 * model against a model, the scenery or a particle; a shadow is not drawn over what stands higher above its floor
		 * than its height, and over what it is drawn on counts by that height rather than the depth. With the scenery
		 * sorted too (the default), nothing between two of the scenery's pieces counts either.
		 */
		EFieldSorting Sorting = EFieldSorting::DepthBufferWithScenery;
	};

	/** The port's per-pixel sorting's measures, in view units (FieldSceneRendering.cpp's and FieldScene.usf's). */
	namespace PortSorting
	{
		/** How far above its floor what lies under a shadow may stand for the shadow to be drawn over it: a character's, a gear's broad one, and either over a model. */
		inline constexpr float CharacterShadowHeight = 64.0f;
		inline constexpr float GearShadowHeight = 256.0f;
		inline constexpr float OverModelShadowHeight = 16.0f;
	}

	/**
	 * Draws a field frame twice in software as its pieces (the scenery's polygons, the characters' quads, the particles,
	 * the gears' polygons and cloths, the children's models), once in the frame's painter's order and once by view
	 * depth, their texels read from VRAM (a transparent texel draws nothing, a semi-transparent one blends), and finds
	 * the pixels where something farther is drawn over something nearer. The scene is the map's the frame was drawn
	 * from; the gears' parts are read through the frame's layout, which must still be alive.
	 */
	FSortingFrameAudit AuditFieldSorting(const FFieldFrame& Frame, const FFieldScene& Scene, const FVram& Vram, const FSortingAuditOptions& Options);

	/** The frame's piece Number (its entry in the frame's order) named for a report: what it is, what it belongs to, which polygon, its entry. */
	std::string DescribeSortingPiece(const FFieldFrame& Frame, const FFieldScene& Scene, uint32 Number, const FSortingPiece& Piece);
}
