#pragma once

#include "Foundation/Types.h"
#include "Graphics/Sprite.h"
#include "Graphics/TextureRegion.h"
#include "Graphics/Vram.h"

#include <functional>
#include <span>

namespace Weltall
{
	class FSpriteFile;

	/** Where a sprite of a file takes its textures and palettes, as its mode places it. */
	struct FSpriteFilePlace
	{
		int32 ClutX = 0;
		int32 ClutY = 0;
		int32 TextureX = 0;
		int32 TextureY = 0;
	};

	/** The palette sets a sprite file holds: its palette block up to the end its header gives, at least one. */
	WELTALLCORE_API int32 SpritePaletteSets(const FSpriteFile& File);

	/** The upload of a file's palette set to its row, as a sprite of it at Place makes it with its first frame. */
	WELTALLCORE_API FVramImage SpritePaletteUpload(const FSpriteFile& File, const FSpriteFilePlace& Place, int32 PaletteSet);

	/** The cell a built part draws from: its texels in its page, read through its CLUT. */
	WELTALLCORE_API FTextureRegion SpritePartCell(const FSpritePart& Part);

	/**
	 * Builds every frame of a file as a sprite of it at Place with palette set PaletteSet shows it, making the frame's
	 * uploads (its palette set with the first, a streamed frame's cells) into Vram, then hands Visit the frame's parts.
	 * A frame the kernel cannot build is left out. Without bOwnPalette the sprite takes the colours VRAM holds at its
	 * CLUT, as a spawned child does, rather than uploading its palette set.
	 */
	WELTALLCORE_API void ForEachSpriteFrame(const FSpriteFile& File, const FSpriteFilePlace& Place, int32 PaletteSet, FVram& Vram,
		const std::function<void(std::span<const FSpritePart>)>& Visit, bool bOwnPalette = true);
}
