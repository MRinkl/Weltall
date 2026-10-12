#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/Vram.h"
#include "Media/DiscIdentification.h"

#include <array>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;

	/** A frame of a texture animation: the image it loads, and the passes of the loop it stays (negative: start again). */
	struct FWorldMapAnimationFrame
	{
		int16 Image = 0;
		int16 Passes = 0;
	};

	/** A rectangle of VRAM the loop loads a scene's images into in turn, and the frames it loads them by. */
	struct FWorldMapTextureAnimation
	{
		FVramRect Rect;
		std::vector<FWorldMapAnimationFrame> Frames;
	};

	/**
	 * What the world map draws its frames with, from the Worldmap program: the planes the terrain's
	 * blocks are tested against, the quadrants each place of the camera in its block leaves out, the
	 * geometry of the sky and of the horizon's band, and the colours the terrain and the models fade
	 * into with distance.
	 */
	struct FWorldMapDrawingTables
	{
		/** The terrain is drawn from the 5 x 5 blocks about the camera's. */
		static constexpr int32 DrawnBlocksAcross = 5;
		static constexpr int32 DrawnBlocks = DrawnBlocksAcross * DrawnBlocksAcross;

		/**
		 * The four planes a block or quadrant is tested against, as view-space normals whose side a
		 * corner is inside of: left, right, top, and a fourth that only asks for a corner ahead of the
		 * eye (the program crosses the wrong vectors for the bottom).
		 */
		std::array<std::array<int32, 3>, 4> FrustumPlanes{};
		/**
		 * [the camera's quarter of its block (bit 0 east, bit 1 south)][block of the 5 x 5, row by row]
		 * [quadrant]: FFFFh where the quadrant is left out, the far row and column of the 10 x 10.
		 */
		std::array<std::array<std::array<uint16, 4>, DrawnBlocks>, 4> QuadrantMasks{};
		/** The sky's four quads, a backdrop turned with the camera but for its yaw. */
		std::array<FGteVector, 16> SkyCorners{};
		/** The horizon's band: two quads side by side. */
		std::array<FGteVector, 8> HorizonCorners{};
		/** A cloud's three layers, a 384-unit square each, 8 units apart; and the same cut into four squares a layer. */
		std::array<FGteVector, 12> CloudCorners{};
		std::array<FGteVector, 48> CloudMiddleCorners{};
		/** The first texel (u, v in a word) of each layer of a cloud: its first look's three, a gap, its second look's three. */
		std::array<uint16, 8> CloudTexels{};
		/** Each particle shape's corners (a quad about its place) and texels (u, v in a word, a corner each); shape 0 draws nothing. */
		std::array<std::array<FGteVector, 4>, 10> ParticleCorners{};
		std::array<std::array<uint16, 4>, 10> ParticleTexels{};
		/**
		 * The animations of the scene's sections 7 (palettes: eight colours of the terrain's nearest fog rows) and 8 (tiles
		 * of the terrain's textures): as many as the program knows rectangles for.
		 */
		std::array<FWorldMapTextureAnimation, 2> PaletteAnimations{};
		std::array<FWorldMapTextureAnimation, 3> TileAnimations{};
		/** Where each place's tile sits on the minimap (x, y from its corner); three are drawn where their objects are. */
		std::array<std::array<int16, 2>, 32> MinimapDots{};
		/** The minimap's arrow: four triangles fanned about the party's place, pointing up. */
		std::array<FGteVector, 12> ArrowCorners{};
		/** The colour the terrain's palettes fade into, and the one the models' palettes do. */
		std::array<uint8, 3> FogColor{};
		std::array<uint8, 3> ObjectFogColor{};
		/** The lights the place models are built with: their directions (rows) and colours (columns). */
		FGteMatrix LightMatrix;
		FGteMatrix ColorMatrix;
		/** The kernel's draw mode for a placed model, by its placement's flags (0-9; the rest are not modes). */
		std::array<int16, 16> DrawModes{};

		/** Reads them from the Worldmap program on a disc of the given release. */
		WELTALLCORE_API static TResult<FWorldMapDrawingTables> Read(const FCdImage& Image, const FGameFileIndex& Index, EDiscRelease Release);
	};
}
