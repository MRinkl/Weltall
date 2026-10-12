#pragma once

#include "CoreMinimal.h"
#include "Rendering/FieldSceneRendering.h"

/**
 * A polygon of a frame the field scene's passes draw from its dynamic corners alone (the world map's, the battle's):
 * where its corners start in the dynamic corners and how many, and what the GPU draws it with.
 */
struct FScenePolygonItem
{
	uint32 FirstCorner = 0;
	uint32 CornerCount = 0;
	bool bSemiTransparent = false;
	bool bTextured = false;
	/** Behind everything else (a sky): under the depth rule, drawn without the depth. */
	bool bBackground = false;
	/** A sprite's part: under the depth rule it marks its pixels, standing at its ordering-table entry. */
	bool bSprite = false;
	/** A shadow on the ground: under the depth rule, drawn only outside the pixels the sprites mark. */
	bool bShadow = false;
	/**
	 * Of the scenery sorted by depth (the world map's ground, place models, trees and clouds, the battle's stage under
	 * EFieldSorting::DepthBufferWithScenery): under the depth rule its opaque texels write their depth beforehand, and it is
	 * drawn against the depth everywhere (with bOverModels, beside the models), the order deciding only within the
	 * scenery's margin, as the field's scenery is.
	 */
	bool bScenery = false;
	/**
	 * A polygon of a 3D model (the arena's gears): under the depth rule its opaque texels mark their pixels with the models'
	 * bit at their own depth, and it is drawn against that depth, sorted by depth against everything that marks or is
	 * sorted with the scenery, as the field's gears are; a shadow is drawn over it only close to its floor.
	 */
	bool bModel = false;
	/**
	 * Drawn in the order whatever the sorting, never against the depth: the arena's effects (by depth a flat quad at a gear's
	 * surface would be cut by it) and its ground (its gears stand on beds under it, FWorldMapFrame::bGearsUnderGround).
	 */
	bool bInOrder = false;
	/**
	 * Drawn in the order over the 3D models (EFieldRunDepth::OverModels), against the sprites as everything else and, of the
	 * scenery sorted by depth, by depth beside the models: the battle's stage, whose floor its gears' soles stand under, and
	 * its effects, which a gear's surface would cut.
	 */
	bool bOverModels = false;
	/** A textured polygon's page's blend mode, which a later untextured semi-transparent polygon blends by too. */
	Weltall::EBlendMode PageBlend = Weltall::EBlendMode::Average;
	/** An untextured polygon drawn after a drawing mode of its own: it sets the blend mode to PageBlend as a textured one does. */
	bool bSetsBlend = false;
};

/** Lays the polygons out in the order given, as the GPU's painter's order draws them. */
void AddScenePolygonsInPaintersOrder(FFieldSceneDraw& Draw, TConstArrayView<FScenePolygonItem> Items);

/**
 * Lays the polygons out under the field's depth rule: the painter's order, but where the sprites' and the models' opaque
 * texels mark the pixels, there the depth decides between them and the rest, each sprite standing at its ordering-table
 * entry; a shadow only outside a sprite's marks, the background behind everything, what keeps the order in it; the scenery
 * sorted by depth (bScenery) against the depth everywhere; what is drawn over the models (bOverModels) in the order there,
 * its scenery's prepass made beside their marks once they are all made.
 */
void AddScenePolygonsUnderDepthRule(FFieldSceneDraw& Draw, TConstArrayView<FScenePolygonItem> Items);
