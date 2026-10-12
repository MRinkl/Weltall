#pragma once

#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/GteMatrix.h"

#include <array>
#include <vector>

namespace Weltall
{
	class FSprite;

	/** Which pieces of a sprite's parts a draw keeps: all of them, or those on one side of a horizontal line through the sprite. */
	enum class ESpriteClip : uint8
	{
		None,
		/** The pieces above the line. */
		KeepAbove,
		/** The pieces below the line. */
		KeepBelow,
	};

	/**
	 * A part of a sprite as the kernel draws it: one textured quad, the matrix the GTE projects it
	 * with and its corners before it, in the order the GPU takes them (the texels' first column and
	 * row at corner 0, their last column at corner 1, their last row at corner 2, both at corner 3),
	 * the texels it shows edge to edge, how they are drawn, and the ordering-table entry the kernel
	 * links it into.
	 */
	struct FSpriteQuad
	{
		FGteMatrix Matrix;
		std::array<FGteVector, 4> Corners{};
		/** The texels' rectangle in the texture page: U0 at corners 0 and 2, U1 at 1 and 3, V0 at 0 and 1, V1 at 2 and 3. */
		int16 U0 = 0;
		int16 U1 = 0;
		int16 V0 = 0;
		int16 V1 = 0;
		uint16 TexturePage = 0;
		uint16 Clut = 0;
		/** The colour, and the GPU code in the top byte (bit 24 raw texture, bit 25 semi-transparent). */
		uint32 ColorCode = 0;
		int32 OtIndex = 0;

		/** A corner in view space: through the matrix, without the GTE's rounding. */
		std::array<float, 3> ViewCorner(std::size_t Corner) const { return ViewPoint(Matrix, Corners[Corner]); }
	};

	/**
	 * The matrix the kernel draws a sprite's parts with: Rotation (the sprite's own matrix as its
	 * mode sets it), and as translation the sprite's position (whole units) through View plus its
	 * pivot at its scale.
	 */
	WELTALLCORE_API FGteMatrix SpriteDrawMatrix(const FSprite& Sprite, const FGteMatrix& Rotation, const FGteMatrix& View);

	/** How far SpriteDrawMatrix moves a sprite's drawing for its pivot: x and y in view units at its scale, x mirrored with its facing. */
	WELTALLCORE_API std::array<int32, 2> SpritePivotOffset(const FSprite& Sprite);

	/**
	 * The matrix the kernel keeps for a sprite with a transform, as it builds it: its rotation (Rx Ry
	 * Rz), each row scaled by the transform's scale (or, built scale first, the rotation times the
	 * scale's diagonal), then each column by half its size factor when it has one; with the sprite's
	 * position (whole units) as translation, as the kernel's drawing of a
	 * model attached to it places it before the view. The kernel builds it when a sprite starts
	 * without keeping its rotation, and again before a drawing once a setter has changed it, so it is
	 * always this.
	 */
	WELTALLCORE_API FGteMatrix SpriteModelMatrix(const FSprite& Sprite);

	/**
	 * The kernel's drawing of a sprite's built frame with Matrix (SpriteDrawMatrix's) into ordering-table
	 * entry OtIndex: each part not in a hidden anchor group becomes a quad, placed by its anchor group's
	 * offset and rotation, appended to Quads when given. Returns the matrix the drawing leaves in the
	 * GTE: the last part's anchor group's.
	 */
	WELTALLCORE_API FGteMatrix DrawSpriteParts(const FSprite& Sprite, const FGteMatrix& Matrix, int32 OtIndex, std::vector<FSpriteQuad>* Quads);

	/**
	 * The kernel's shadow of a sprite: each part not in a hidden anchor group laid flat
	 * on the floor under the sprite, x across and the part's y along the view's depth, through the view's
	 * rows scaled by the sprite's scale, half of it and nothing (so every corner lies at the floor point's
	 * depth), black (code 2Ch), into entry OtIndex. Its texels run from the part's first to its last. Once
	 * projected, the GPU's first two corners take their average y, and the last two theirs.
	 */
	WELTALLCORE_API void DrawSpriteShadow(const FSprite& Sprite, const FGteMatrix& View, int32 OtIndex, std::vector<FSpriteQuad>& Quads);

	/**
	 * The kernel's drawing of the pieces of a sprite's parts on one side of the line Line texels below
	 * its origin, into ordering-table entry OtIndex: the parts cut at whole texels, without the pivot,
	 * the anchor groups or the hidden groups (the field's wading).
	 */
	WELTALLCORE_API void DrawSpritePartsClipped(const FSprite& Sprite, const FGteMatrix& Matrix, int32 OtIndex, ESpriteClip Clip, int32 Line,
		std::vector<FSpriteQuad>& Quads);
}
