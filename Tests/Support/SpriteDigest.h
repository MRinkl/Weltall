#pragma once

#include "Graphics/Sprite.h"
#include "Support/OracleDigest.h"

namespace Weltall::Testing
{
	/**
	 * A sprite as the sprite oracles digest one: its core, its
	 * transform with its anchors or attached model, its extra block, and its built parts. Offsets
	 * into the sprite's script file stand for the kernel's addresses. bDrawn: the original has drawn
	 * the sprite since, building its matrix and clearing the matrix-dirty bit, which the port leaves
	 * to its renderer: the bit is left out.
	 */
	void DigestSprite(FOracleDigest& Digest, const FSprite& Sprite, bool bDrawn = false);
}
