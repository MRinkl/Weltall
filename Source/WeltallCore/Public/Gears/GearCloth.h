#pragma once

#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/MdlModel.h"

#include <array>
#include <optional>
#include <vector>

namespace Weltall
{
	struct FGearClothMesh;
	struct FGearClothSpec;

	/** A point of a cloth's column: the link to the next point (0 ends the column), its pull, where it is, its normal. */
	struct FGearClothPoint
	{
		int16 Length = 0;
		int16 Pull = 0;
		FGteVector Position{};
		/** The triangles whose normals the step added up, before it divides. */
		int16 Normals = 0;
		std::array<int32, 3> Normal{};
	};

	/** A sphere the cloth stays out of: where it sits on its bone, and where that is in the model's frame this tick. */
	struct FGearClothSphereState
	{
		FGteVector Local{};
		int16 Bone = 0;
		FGteVector World{};
		int16 Radius = 0;
	};

	struct FGearClothTriangle
	{
		std::array<uint16, 3> Points{};
		std::array<FMdlUv, 3> Uvs{};
	};

	/**
	 * A cloth hung from a bone of a gear: columns of linked points, each column's first point on its
	 * anchor, the rest pulled down and by the wind, kept above the floor and out of the spheres; its
	 * triangles are drawn front or back coloured, lit by each point's averaged normal. What the drawing
	 * of each tick leaves is its points; nothing in the gear's logic reads them.
	 */
	struct FGearCloth
	{
		int16 Bone = 0;
		std::array<uint8, 3> Front{};
		std::array<uint8, 3> Back{};
		/** Each column's anchor in the bone's frame. */
		std::vector<FGteVector> Anchors;
		std::vector<FGearClothSphereState> Spheres;
		/** Where each column's points start in Points: its first is on its anchor, its last ends it. */
		std::vector<uint16> ColumnStarts;
		std::vector<FGearClothPoint> Points;
		std::vector<FGearClothTriangle> Triangles;
		/** The 4-bit page (blend mode 1) and CLUT the triangles are drawn with. */
		uint16 TexturePage = 0;
		uint16 Clut = 0;
	};

	/**
	 * The cloth as the model's load makes it, its texture at (TextureX, TextureY) and its CLUT at
	 * (ClutX, ClutY) the load's places plus the description's; nothing where the original would divide
	 * by zero (a single column, or a column of no points beside another) or leave triangles unmade.
	 */
	WELTALLCORE_API std::optional<FGearCloth> MakeGearCloth(const FGearClothSpec& Spec, const FGearClothMesh& Mesh, int16 TextureX, int16 TextureY, int16 ClutX, int16 ClutY);

	/**
	 * One tick of the cloth, its anchors and spheres already placed: each link pulled toward the next
	 * point by the wind and its pull, cut to its length times the model's Scale, kept above Floor and
	 * pushed out of the spheres; then every point's normal averaged from its triangles. False where a
	 * point in no triangle divides by zero (the original traps).
	 */
	[[nodiscard]] WELTALLCORE_API bool StepGearCloth(FGearCloth& Cloth, const std::array<int16, 3>& Wind, int32 Scale, int16 Floor);

	/** A triangle of a gear's cloth as a tick drew it: its corners in the world, lit on the side the screen shows. */
	struct FGearDrawnClothTriangle
	{
		std::array<FGteVector, 3> Points{};
		std::array<FMdlColor, 3> Colors{};
		std::array<FMdlUv, 3> Uvs{};
		uint16 TexturePage = 0;
		uint16 Clut = 0;
		uint16 Bucket = 0;
		/** Which triangle of which cloth of which slot's model. */
		uint8 Slot = 0;
		uint8 Cloth = 0;
		uint16 Triangle = 0;
		/** The same point of the same cloth from tick to tick, under the same layout: the display pairs corners by them. */
		std::array<uint32, 3> PointKeys{};
	};

	/** The cloths' triangles are Gouraud-shaded textured triangles, modulated. */
	inline constexpr uint8 GearClothCode = 0x34;

	/** Which of a cloth's triangles its drawing keeps. */
	enum class EGearClothCulling : uint8
	{
		/**
		 * The original's: the triangles in order up to the first with a corner whose depth the GTE cannot hold (behind
		 * the eye or past its range). The original's loop stays on a triangle it leaves out, so that one and every one
		 * after it are left out of the frame.
		 */
		Original,
		/** Every triangle wherever its corners lie: the drawing clips them. */
		InRange,
	};

	/**
	 * The cloth's triangles as the gears' drawing draws them under Gte (its transform the view, its light
	 * matrix the one the last part drawn left): those Culling keeps, sorted by their average depth shifted
	 * by OrderingShift into an entry up to FarBucket, lit on the side the screen shows, appended to Drawn
	 * in the order they are linked. Slot and ClothNumber name them.
	 */
	WELTALLCORE_API void DrawGearCloth(const FGearCloth& Cloth, const FGte& Gte, EGearClothCulling Culling, int32 FarBucket, int32 OrderingShift, uint8 Slot,
		uint8 ClothNumber, std::vector<FGearDrawnClothTriangle>& Drawn);
}
