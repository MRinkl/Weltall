#pragma once

#include "Battle/Show/BattleShowFrame.h"
#include "Field/FieldRenderOptions.h"
#include "Support/SortingDrawing.h"

#include <string>

namespace Weltall::Testing
{
	struct FBattleSortingAuditOptions
	{
		/** The view's width in the original's screen pixels about its centre (320: the original's screen; 398: 16:9 at its 224 lines). */
		int32 Width = 398;
		/** How many audit pixels across and down each screen pixel is drawn at. */
		int32 Scale = 2;
		/** How much nearer than what the order draws over it a piece must stand to count, in view units. */
		float Margin = 16.0f;
		/**
		 * The sorting judged as drawing the frame (BattleFrameDraw.cpp). The painter's order: as the original draws. Either
		 * depth sorting: the sprites stand at their entries' depth against everything and their shadows are kept off them,
		 * so nothing against a sprite counts, nor a sprite's shadow (a card at its feet's depth); the 3D objects are sorted
		 * among themselves and a shadow is drawn over them only close to its floor; the stage and the effects keep the
		 * order, over the 3D objects too. With the scenery sorted too (the default), the stage's pieces are sorted among
		 * themselves beside the 3D objects, and a shadow is not drawn over what of the stage stands higher than a broad
		 * shadow's height above its floor.
		 */
		EFieldSorting Sorting = EFieldSorting::DepthBufferWithScenery;
	};

	/**
	 * Draws the scene of a battle's frame (what the frame links before its fade or its HUD) twice in software as its pieces
	 * (the stage's polygons, the 3D objects' polygons, cloths and floor shadows, the sprites' and their shadows' quads, the
	 * target markers, the sparks, lines, trails and the effects' models; the backdrop left out), once in the frame's order
	 * and once by view depth, their texels read from the frame's VRAM, and finds the pixels where something farther is
	 * drawn over something nearer.
	 */
	FSortingFrameAudit AuditBattleSorting(const FBattleShowFrame& Frame, const FBattleSortingAuditOptions& Options);

	/** The frame's piece Number (its link in the frame's order) named for a report: what it is, what it belongs to, which polygon, its entry. */
	std::string DescribeBattleSortingPiece(const FBattleShowFrame& Frame, uint32 Number, const FSortingPiece& Piece);
}
