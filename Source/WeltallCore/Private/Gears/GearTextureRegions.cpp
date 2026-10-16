#include "Gears/GearTextureRegions.h"

#include "Gears/GearFiles.h"
#include "Graphics/Vram.h"

#include <algorithm>
#include <memory>

namespace Weltall
{
	std::array<int16, 2> GearTextureBlockCorner(const FGearTextureBlock& Block, const FGearTexturePlace* Place)
	{
		const int16 BaseX = Place != nullptr ? (Block.bClut ? Place->ClutX : Place->X) : Block.X;
		const int16 BaseY = Place != nullptr ? (Block.bClut ? Place->ClutY : Place->Y) : Block.Y;
		return {static_cast<int16>(BaseX + Block.OffsetX), static_cast<int16>(BaseY + Block.OffsetY)};
	}

	void UploadGearTextures(FVram& Vram, std::span<const FGearTextureBlock> Blocks, const FGearTexturePlace* Place)
	{
		const bool bPortRows = Place != nullptr && Place->Y >= FVram::Height;
		FVramImage Image;
		for (const FGearTextureBlock& Block : Blocks)
		{
			const std::array<int16, 2> Corner = GearTextureBlockCorner(Block, Place);
			Image.Rect = {Corner[0], Corner[1], Block.Width, Block.Height};
			Image.Units = Block.Pixels;
			if (bPortRows)
			{
				Vram.UploadToPortRows(Image);
			}
			else
			{
				Vram.Upload(Image);
			}
		}
	}

	std::optional<std::vector<FGearCloth>> MakeGearCloths(const FGearModelPack& Pack, const FGearTexturePlace& Place)
	{
		std::vector<FGearCloth> Cloths;
		for (std::size_t Cloth = 0; Cloth < Pack.Header.Cloths; ++Cloth)
		{
			if (Cloth >= Pack.Cloths.size() || Cloth >= Pack.ClothMeshes.size())
			{
				return std::nullopt;
			}
			const FGearClothSpec& Spec = Pack.Cloths[Cloth];
			std::optional<FGearCloth> Made = MakeGearCloth(Spec, Pack.ClothMeshes[Cloth], static_cast<int16>(Place.X + Spec.TextureX),
				static_cast<int16>(Place.Y + Spec.TextureY), static_cast<int16>(Place.ClutX + Spec.ClutX), Place.ClutY);
			if (!Made)
			{
				return std::nullopt;
			}
			Cloths.push_back(std::move(*Made));
		}
		return Cloths;
	}

	std::vector<FTextureRegion> GearTextureRegions(const FMdlModel& Model, std::span<const FGearCloth> Cloths, const FGearTexturePlace* Place)
	{
		const auto Footprint = [](uint32 View, std::span<const FMdlUv> Uvs)
		{
			uint8 FirstU = 255, FirstV = 255, LastU = 0, LastV = 0;
			for (const FMdlUv& Uv : Uvs)
			{
				FirstU = std::min(FirstU, Uv.U);
				FirstV = std::min(FirstV, Uv.V);
				LastU = std::max(LastU, Uv.U);
				LastV = std::max(LastV, Uv.V);
			}
			return TextureRegionBetween(View, FirstU, FirstV, LastU, LastV);
		};
		// Only a place in the port's rows gives the parts' pages and CLUTs high y bits; elsewhere they are read as the console reads them.
		const bool bPortRows = Place != nullptr && Place->Y >= FVram::Height;
		std::vector<FTextureRegion> Footprints;
		for (const FMdlPart& Part : Model.Parts)
		{
			for (const FMdlPrimitive& Primitive : Part.Primitives)
			{
				const FMdlPrimitiveTraits& Traits = Primitive.Traits();
				if (!Traits.bTextured || Primitive.Type == EMdlPrimitiveType::EnvironmentMappedTriangle)
				{
					continue;
				}
				const FMdlTextureState Texture = PlaceGearTexture(Primitive, Place);
				const uint32 View = bPortRows
					? PackTextureView(FTexturePage::FromPortAttribute(Texture.TexturePage), FClutPosition::FromPortAttribute(Texture.Clut))
					: PackTextureView(FTexturePage::FromAttribute(Texture.TexturePage), FClutPosition::FromAttribute(Texture.Clut));
				Footprints.push_back(Footprint(View, std::span<const FMdlUv>(Primitive.Uvs.data(), Traits.Corners)));
			}
		}
		for (const FGearCloth& Cloth : Cloths)
		{
			// A cloth's page and CLUT are made from its place: their high y bits are set only for one in the port's rows.
			const uint32 View = PackTextureView(FTexturePage::FromPortAttribute(Cloth.TexturePage), FClutPosition::FromPortAttribute(Cloth.Clut));
			for (const FGearClothTriangle& Triangle : Cloth.Triangles)
			{
				Footprints.push_back(Footprint(View, Triangle.Uvs));
			}
		}
		return MergeTextureRegions(std::move(Footprints));
	}

	std::vector<FTexturePictureSeed> GearTextureSeeds(const FGearModelPack& Pack, const FGearTexturePlace* Place, std::span<const FTextureRegion> Regions,
		std::span<const FGearTextureBlock> Over, const FGearTexturePlace* OverPlace)
	{
		std::vector<FTexturePictureSeed> Seeds;
		Seeds.reserve(Regions.size());
		std::vector<std::pair<FClutPosition, std::shared_ptr<const std::vector<uint16>>>> Palettes;
		for (const FTextureRegion& Region : Regions)
		{
			const FTexturePage Page = TextureViewPage(Region.View);
			if (Page.Depth == ETextureDepth::Direct15)
			{
				Seeds.push_back({Region});
				continue;
			}
			const FClutPosition Clut = TextureViewClut(Region.View);
			const int32 Count = Page.Depth == ETextureDepth::Indexed4 ? 16 : 256;
			auto Found = std::find_if(Palettes.begin(), Palettes.end(), [&Clut, Count](const auto& Entry)
			{
				return Entry.first == Clut && (Entry.second == nullptr || static_cast<int32>(Entry.second->size()) == Count);
			});
			if (Found == Palettes.end())
			{
				std::vector<uint16> Colors(static_cast<std::size_t>(Count));
				std::vector<bool> Held(static_cast<std::size_t>(Count));
				const auto Take = [&Clut, Count, &Colors, &Held](std::span<const FGearTextureBlock> Blocks, const FGearTexturePlace* At)
				{
					for (const FGearTextureBlock& Block : Blocks)
					{
						const std::array<int16, 2> Corner = GearTextureBlockCorner(Block, At);
						if (Clut.Y < Corner[1] || Clut.Y >= Corner[1] + Block.Height)
						{
							continue;
						}
						for (int32 Index = 0; Index < Count; ++Index)
						{
							const int32 Column = Clut.X + Index - Corner[0];
							if (Column >= 0 && Column < Block.Width)
							{
								Colors[static_cast<std::size_t>(Index)] = Block.Pixels[static_cast<std::size_t>((Clut.Y - Corner[1]) * Block.Width + Column)];
								Held[static_cast<std::size_t>(Index)] = true;
							}
						}
					}
				};
				Take(Pack.Textures, Place);
				Take(Over, OverPlace);
				const bool bWhole = std::all_of(Held.begin(), Held.end(), [](bool bHeld) { return bHeld; });
				Palettes.emplace_back(Clut, bWhole ? std::make_shared<const std::vector<uint16>>(std::move(Colors)) : nullptr);
				Found = Palettes.end() - 1;
			}
			Seeds.push_back({Region, Found->second});
		}
		// The texels as the load leaves them, in a VRAM of their own.
		const auto Loaded = std::make_unique<FVram>();
		UploadGearTextures(*Loaded, Pack.Textures, Place);
		UploadGearTextures(*Loaded, Over, OverPlace);
		for (FTexturePictureSeed& Seed : Seeds)
		{
			if (TextureViewPage(Seed.Region.View).Depth == ETextureDepth::Direct15)
			{
				Seed.Key = TextureRegionKey(*Loaded, Seed.Region);
			}
			else if (Seed.Palette)
			{
				Seed.Key = TextureRegionKeyWith(ReadTextureRegionTexels(*Loaded, Seed.Region), *Seed.Palette);
			}
		}
		return Seeds;
	}

	std::vector<FTexturePictureSeed> UploadGearModelTextures(FVram& Vram, const FGearModelPack& Pack, const FGearTexturePlace* Place,
		std::span<const FGearTextureBlock> Over, const FGearTexturePlace* OverPlace)
	{
		const bool bPlaced = Place != nullptr && (Pack.Header.Flags & GearModelFlags::AbsoluteTextures) == 0;
		UploadGearTextures(Vram, Pack.Textures, bPlaced ? Place : nullptr);
		UploadGearTextures(Vram, Over, OverPlace);
		const std::optional<std::vector<FGearCloth>> Cloths = MakeGearCloths(Pack, Place != nullptr ? *Place : FGearTexturePlace{});
		const std::vector<FTextureRegion> Regions = GearTextureRegions(Pack.Model, Cloths ? std::span<const FGearCloth>(*Cloths) : std::span<const FGearCloth>(),
			bPlaced ? Place : nullptr);
		return GearTextureSeeds(Pack, bPlaced ? Place : nullptr, Regions, Over, OverPlace);
	}
}
