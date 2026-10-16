#pragma once

#include "Graphics/TextureRegion.h"

#include <vector>

namespace Weltall
{
	struct FFieldMap;
	struct FFieldScene;

	/**
	 * The pictures a map's scenery is drawn from: for each texture page and CLUT, the texel bounds of every polygon
	 * that reads them, hidden models' included, merged where they overlap or meet.
	 */
	WELTALLCORE_API std::vector<FTextureRegion> FieldSceneryTextureRegions(const FFieldScene& Scene);

	/**
	 * The pictures a map's resident sprites are drawn from: for each of its sprite files whose texels the map's textures
	 * hold, the rectangle around every frame's cells under each palette set, per page and CLUT, so that the file's sheet
	 * is one picture. A streamed sprite's cells come and go with its frames and are pictures of their own.
	 */
	WELTALLCORE_API std::vector<FTextureRegion> FieldSpriteSheetRegions(const FFieldMap& Map);

	/**
	 * The pictures of the panoramas a map's scripts can make: the strip of every FE 80 its threads reach, where they reach
	 * an FE 82 too (which has the map make the panorama after its init). None where the scripts cannot be read.
	 */
	WELTALLCORE_API std::vector<FTextureRegion> FieldPanoramaTextureRegions(const FFieldMap& Map);
}
