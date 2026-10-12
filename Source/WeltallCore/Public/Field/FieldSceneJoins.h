#pragma once

#include "Field/FieldScene.h"

#include <vector>

namespace Weltall
{
	/**
	 * Where the pieces of a map's scenery are drawn to meet. The maps place their models at whole units and by
	 * rotations of 12 bits, so a piece set into another (a door in its frame, a floor beside the next) often stands
	 * a fraction of a unit off it, or a whole unit: the original's whole pixels mostly close the gap, a drawing at
	 * any resolution shows it as a hairline of whatever lies behind.
	 *
	 * An edge of a model is open when no other polygon of the model shares it. Two open edges of different models
	 * make a seam when they run beside each other no farther apart than a unit and a half, for a unit and a third
	 * of the shorter one at least, their polygons not lying to the same side (one over the other). The model of
	 * fewer polygons (the later one of two alike) is fitted to the other: each end of its edge goes to the nearest
	 * end of the other's within that reach, else onto the other's edge where it lies beside it. A vertex follows
	 * the one it is joined to wherever that one is itself joined, and a vertex standing where a joined one of
	 * another model stands follows it, so no join opens a gap of its own.
	 *
	 * Models turned to face the screen or moved by a vertex animation take no part. The joins are ordered by object
	 * and vertex, and hold while both models stand as the map's data places them (FFieldSceneObject::IsAsBuilt).
	 */
	WELTALLCORE_API std::vector<FFieldSceneJoin> BuildFieldSceneJoins(const FFieldScene& Scene);
}
