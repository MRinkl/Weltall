#pragma once

#include "Field/FieldCamera.h"
#include "Field/FieldCharacters.h"
#include "Field/FieldSceneFrame.h"
#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/MdlModel.h"
#include "Graphics/MdlPackets.h"

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	class FSpriteFile;
	struct FFieldFog;

	/**
	 * A model part a spawned child draws, as its sprite's script attached it: parsed where the script
	 * points, its packets built without light (as the kernel's attach builds them, both buffers alike),
	 * and each polygon's page and CLUT, the page's place the one the script set before the build.
	 */
	struct FFieldSpriteModelPart
	{
		const FSpriteFile* File = nullptr;
		uint32 Offset = 0;
		bool bTextureOverride = false;
		int16 TextureX = 0;
		int16 TextureY = 0;
		FMdlPart Part;
		std::vector<FMdlPacket> Packets;
		std::vector<FMdlTextureState> Textures;
	};

	/** The parts the children of a map have drawn, each built the first time: kept until the next map. */
	class FFieldSpriteModelParts
	{
	public:
		/**
		 * The part a drawing names. Fails for a part that cannot be read, and for a textured polygon
		 * before the part's first page or CLUT command, which would take the builder's from whatever
		 * it built last.
		 */
		[[nodiscard]] WELTALLCORE_API TResult<const FFieldSpriteModelPart*> Find(const FFieldSpriteModelDraw& Draw);

		void Clear() { Parts.clear(); }

	private:
		std::vector<std::unique_ptr<FFieldSpriteModelPart>> Parts;
	};

	/** A child's model as the kernel's drawing drew it in a tick: its part, and the drawing's matrix, fog and key. */
	struct FFieldSpriteModelDrawn
	{
		const FFieldSpriteModelPart* Part = nullptr;
		/** Model to world: SpriteModelMatrix's. */
		FGteMatrix Matrix;
		bool bFogged = false;
		uint32 Key = 0;
	};

	/** A child's model as a tick drew it under a view. */
	struct FFieldSpriteModelView
	{
		/** Model to view space, as the GTE drew it. */
		FGteMatrix Matrix;
		/** Its sprite's place in the world, whole units: one that jumps between ticks is not drawn in between. */
		std::array<int32, 3> World{};
	};

	/** A polygon of a child's model as a tick drew it. */
	struct FFieldSpriteModelPolygon
	{
		/** Its model in FFieldSpriteModelsFrame::Models. */
		uint32 Model = 0;
		/** The same child's same polygon from tick to tick. */
		uint32 BlendKey = 0;
		EMdlPrimitiveType Type = EMdlPrimitiveType::FlatTriangle;
		uint8 CornerCount = 3;
		/** Its corners in its model's space, and on the 320 x 224 screen as the GTE projects them. */
		std::array<FGteVector, 4> Corners{};
		std::array<int16, 4> ScreenX{};
		std::array<int16, 4> ScreenY{};
		std::array<FMdlUv, 4> Uvs{};
		/** The GPU's code as the packet draws it: a fog handler clears its raw bit. */
		uint8 Code = 0;
		/** The page and CLUT attributes, for the textured kinds. */
		uint16 TexturePage = 0;
		uint16 Clut = 0;
		/** One per corner for the Gouraud kinds: as built, or depth-cued in fog. */
		std::array<FMdlColor, 4> Colors{};
		/** Depth-cued in fog: its texels are modulated by its colour. */
		bool bFogged = false;
		/** The ordering-table entry it is linked into, why the original leaves it out (FieldPolygonDrop), and whether it is drawn. */
		uint16 Bucket = 0;
		uint8 Dropped = 0;
		bool bDrawn = false;
	};

	/**
	 * What the children's models draw in one tick, in the order the kernel links them: the children
	 * newest first, each part's polygons in order (within an entry the last linked is drawn first).
	 * Kept from tick to tick: it allocates only while it grows.
	 */
	struct FFieldSpriteModelsFrame
	{
		std::vector<FFieldSpriteModelView> Models;
		std::vector<FFieldSpriteModelPolygon> Polygons;
		/** Scratch: a part's vertices projected. */
		std::vector<FGteProjection> Projected;
	};

	/**
	 * The children's models of a tick as the kernel draws them (its drawing of a model under the view,
	 * without the bounds' culling, in draw mode 0, or the fog's 4 for a sprite made on a fogged map):
	 * every polygon projected, tested and sorted as the field's scenery is (in fog the flat textured
	 * kinds depth-cued), and left out past the far bucket.
	 */
	WELTALLCORE_API void EvaluateFieldSpriteModels(std::span<const FFieldSpriteModelDrawn> Models, const FFieldView& View, const FFieldFog& Fog, int32 FarBucket,
		EFieldCulling Culling, FFieldSpriteModelsFrame& Frame);
}
