#pragma once

#include "Gears/GearCloth.h"
#include "Gears/GearPackets.h"
#include "Graphics/TexturePictures.h"
#include "Graphics/TextureRegion.h"

#include <array>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	struct FGearModelPack;
	struct FGearTextureBlock;

	/** Where a model's load puts one of its texture blocks: Place's image or CLUT corner plus the block's offset, or where the file says (Place null). */
	WELTALLCORE_API std::array<int16, 2> GearTextureBlockCorner(const FGearTextureBlock& Block, const FGearTexturePlace* Place);

	/**
	 * Uploads a model's texture blocks as its load does with its textures at Place (null: where the file says), into the
	 * port's rows for a place there.
	 */
	WELTALLCORE_API void UploadGearTextures(FVram& Vram, std::span<const FGearTextureBlock> Blocks, const FGearTexturePlace* Place);

	/**
	 * The cloths a model's load makes with its textures at Place, as many as its header asks for (MakeGearCloth: each
	 * texture at the place plus the description's); nothing where one cannot be made.
	 */
	WELTALLCORE_API std::optional<std::vector<FGearCloth>> MakeGearCloths(const FGearModelPack& Pack, const FGearTexturePlace& Place);

	/**
	 * The texture regions of a model whose load put its textures at Place (null: where the file says), as the Battle
	 * program and the field read its pages and CLUTs (with their high y bits for a place in the port's rows): the texels
	 * of every textured polygon of every part and of every triangle of its cloths, merged per view. An environment-mapped
	 * polygon's texels follow the view and are left out. A model's regions are the same wherever its load puts it, so
	 * they name its pictures as the texture export saves them.
	 */
	WELTALLCORE_API std::vector<FTextureRegion> GearTextureRegions(const FMdlModel& Model, std::span<const FGearCloth> Cloths, const FGearTexturePlace* Place);

	/**
	 * Regions with the colours the model's own texture blocks, put at Place (null: where the file says), and then Over (the
	 * images and CLUTs a variant uploads at OverPlace after the model's) give their CLUTs, the later block over the earlier:
	 * the colours the load left, whatever later changes them. A region whose CLUT the blocks do not hold whole has none,
	 * nor has a direct one. Each takes the key of its picture as the load leaves its texels, Over over the model's blocks;
	 * none for an indexed one without colours.
	 */
	WELTALLCORE_API std::vector<FTexturePictureSeed> GearTextureSeeds(const FGearModelPack& Pack, const FGearTexturePlace* Place, std::span<const FTextureRegion> Regions,
		std::span<const FGearTextureBlock> Over = {}, const FGearTexturePlace* OverPlace = nullptr);

	/**
	 * A model loaded into Vram as a load that puts its textures at Place does (where its file says when Place is null or
	 * its textures are absolute; its cloths at Place either way), Over (what the making uploads after it: a variant's images
	 * or its parts' textures) after it at OverPlace, and its texture regions with their loaded colours and keys
	 * (GearTextureRegions, GearTextureSeeds): its pictures as the texture export keys them.
	 */
	WELTALLCORE_API std::vector<FTexturePictureSeed> UploadGearModelTextures(FVram& Vram, const FGearModelPack& Pack, const FGearTexturePlace* Place,
		std::span<const FGearTextureBlock> Over = {}, const FGearTexturePlace* OverPlace = nullptr);
}
