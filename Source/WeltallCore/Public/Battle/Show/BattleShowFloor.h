#pragma once

#include "Battle/Show/BattleStageFiles.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"

#include <optional>
#include <vector>

namespace Weltall
{
	/**
	 * The stage's floor mesh as the battle searches it: the triangle under a point, looked for among a
	 * known triangle's neighbours first (each search marking the triangles it tried with its own number,
	 * which wraps after 255 searches and clears every mark), then over the whole mesh; and the height of
	 * the triangle's plane there.
	 */
	class FBattleFloor
	{
	public:
		/** The stage's mesh; a stage without one has no floor. */
		WELTALLCORE_API void Reset(const std::vector<FBattleFloorVertex>& Vertices, const std::vector<FBattleFloorTriangle>& Triangles);

		bool HasFloor() const { return !Corners.empty() && !Faces.empty(); }
		int32 TriangleCount() const { return static_cast<int32>(Faces.size()); }

		/**
		 * The triangle under Point among Known's neighbours up to Depth steps away,
		 * the search run Depth times; -1 when none (or no floor, or Known past the mesh). With a Depth of 0
		 * the original returns a register it never set: nothing.
		 */
		WELTALLCORE_API std::optional<int32> FindTriangle(const FGteVector& Point, int32 Known, int32 Depth);
		/** The first triangle under Point, -1 for none. */
		WELTALLCORE_API int32 ScanTriangles(const FGteVector& Point) const;
		/**
		 * Point's height on Triangle's plane written into its y, and the triangle's
		 * attribute; -1 (nothing written) for no floor or a negative triangle.
		 */
		WELTALLCORE_API int32 Height(FGteVector& Point, int32 Triangle) const;
		/** A sprite's snap to the floor's search: the triangle under Point from Known, else over the whole mesh. */
		WELTALLCORE_API int32 TriangleUnder(const FGteVector& Point, int32 Known);

		/** Each triangle's mark and the search number (for tests of the searches' bookkeeping). */
		const std::vector<uint8>& Marks() const { return TriedMarks; }
		uint8 SearchNumber() const { return Search; }

	private:
		bool Inside(int32 Triangle, const FGteVector& Point) const;
		int32 SearchNeighbours(const FGteVector& Point, int32 Triangle, int32 Depth);

		std::vector<FBattleFloorVertex> Corners;
		std::vector<FBattleFloorTriangle> Faces;
		std::vector<uint8> TriedMarks;
		uint8 Search = 1;
	};
}
