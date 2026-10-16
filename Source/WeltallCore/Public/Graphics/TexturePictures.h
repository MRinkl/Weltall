#pragma once

#include "Graphics/TextureRegion.h"
#include "Graphics/Vram.h"

#include <memory>
#include <span>
#include <vector>

namespace Weltall
{
	/**
	 * A picture with the colours it comes with: one VRAM holds (FVramPicture), or a region of a model as its load left its
	 * CLUT. It is what the texture export saves and what teaches a replacement its palette, whatever CLUT the drawing
	 * reads the picture through later (a fade, a flash, palette animation).
	 */
	struct FTexturePictureSeed
	{
		FTextureRegion Region;
		/** None for a direct picture. */
		std::shared_ptr<const std::vector<uint16>> Palette{};
		/**
		 * A model's picture: its key as the model's load left its texels (0: none, keyed as drawn), which names its
		 * replacement whatever the model's own animations later write over them.
		 */
		uint64 Key = 0;
	};

	/**
	 * The regions of a view that pictures lie in: each picture of the view's depth, as far as the view's page reaches it,
	 * a part of a larger picture standing for all of it. Appended to Regions.
	 */
	WELTALLCORE_API void AddPictureRegionsInView(std::span<const FVramPicture> Pictures, uint32 View, std::vector<FTextureRegion>& Regions);

	/**
	 * Each picture as one region, in the view of the page its first column and row lie in: a picture larger than that
	 * page is the part of it the page reaches, standing for all of it. Appended to Seeds.
	 */
	WELTALLCORE_API void AddPictureSeeds(std::span<const FVramPicture> Pictures, std::vector<FTexturePictureSeed>& Seeds);
}
