#pragma once

#include "Battle/Show/BattleShowExtensions.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/Sprite.h"
#include "Graphics/SpriteDrawing.h"

#include <vector>

namespace Weltall
{
	/**
	 * The animation extensions' drawings of a sprite's parts: the GTE they
	 * project with, the sprites' view, and the quads they link, each with its ordering-table entry, in the order
	 * they link them. A quad's texels are the packet's as given: the far ones a wave's edge, a scroll's last texel.
	 */
	struct FBattleExtensionDrawing
	{
		FGte* Gte = nullptr;
		const FGteMatrix* View = nullptr;
		std::vector<FSpriteQuad>* Quads = nullptr;

		/**
		 * EXT2's wave: the sprite's first part in strips four texels high from its bottom up, each strip's top edge shifted
		 * across by the wave at its row and its bottom edge by the row's below. False where the original reads its parent's
		 * entry through a register it never set, or a part where a model's packets lie.
		 */
		WELTALLCORE_API bool DrawWave(FSprite& Sprite, const FBattleWave& Wave);

		/**
		 * EXT4's and EXT5's scrolls: copies of the sprite's parts side by side along x. EXT4's from its scroll's place
		 * outward both ways until a copy's near edge leaves the screen, all in the sprite's entry; EXT5's sixteen from one
		 * width before its scroll's place, the sprite put back where it began, each part by its own depth and clipped at
		 * the sprite's origin. False where the original does not end (parts of no width, copies that never leave the
		 * screen), divides by no width, or reads its parent's entry through a register it never set.
		 */
		WELTALLCORE_API bool DrawScroll(FSprite& Sprite, const FBattleScroll& Scroll);

	private:
		/** The sprite's entry and matrix as the kernel's sprite drawing sets them; false when nothing is drawn (bFault: the port stops). */
		bool SetUp(FSprite& Sprite, int32& Entry, FGteMatrix& Matrix, bool& bFault);
		/** A copy of the sprite's parts moved by (Dx, Dy) texels, into Entry, or each part by its depth with Entry below 0. */
		void AddCopy(const FSprite& Sprite, const FGteMatrix& Matrix, int32 Dx, int32 Dy, int32 Entry);
	};
}
