#include "Field/FieldTextureRegions.h"

#include "Field/EventAssembly.h"
#include "Field/EventScripts.h"
#include "Field/FieldBackdrop.h"
#include "Field/FieldMap.h"
#include "Field/FieldScene.h"
#include "Field/FieldSpriteFiles.h"
#include "Field/FieldSprites.h"
#include "Graphics/SpriteCells.h"

#include <algorithm>
#include <cstring>

namespace Weltall
{
	std::vector<FTextureRegion> FieldSceneryTextureRegions(const FFieldScene& Scene)
	{
		std::vector<FTextureRegion> Footprints;
		Footprints.reserve(Scene.Polygons.size());
		for (const FFieldScenePolygon& Polygon : Scene.Polygons)
		{
			const FMdlPrimitive& Primitive = Scene.PrimitiveOf(Polygon);
			const FMdlPrimitiveTraits& Traits = Primitive.Traits();
			if (!Traits.bTextured)
			{
				continue;
			}
			uint8 FirstU = 255, FirstV = 255, LastU = 0, LastV = 0;
			for (std::size_t Corner = 0; Corner < Traits.Corners; ++Corner)
			{
				const FMdlUv& Uv = Primitive.Uvs[Corner];
				FirstU = std::min(FirstU, Uv.U);
				FirstV = std::min(FirstV, Uv.V);
				LastU = std::max(LastU, Uv.U);
				LastV = std::max(LastV, Uv.V);
			}
			Footprints.push_back(TextureRegionBetween(PackTextureView(Polygon.Page, Polygon.Clut), FirstU, FirstV, LastU, LastV));
		}
		return MergeTextureRegions(std::move(Footprints));
	}

	std::vector<FTextureRegion> FieldSpriteSheetRegions(const FFieldMap& Map)
	{
		const TResult<std::vector<FSpriteFile>> Files = ParseFieldSpriteFiles(Map.SpriteSets);
		if (!Files)
		{
			return {};
		}
		std::vector<FTextureRegion> Sheets;
		std::vector<FTextureRegion> Footprints;
		FVram Palettes;
		for (std::size_t Number = 0; Number < Files->size() && Number < Map.SpriteSlots.size(); ++Number)
		{
			const FSpriteFile& File = (*Files)[Number];
			if (!File.IsResident(File.Tables()))
			{
				continue;
			}
			Footprints.clear();
			for (int32 Set = 0; Set < SpritePaletteSets(File); ++Set)
			{
				const FSpriteFilePlace Place = FieldMapSpritePlace(Map.SpriteSlots[Number], static_cast<int32>(Number), Set);
				ForEachSpriteFrame(File, Place, Set, Palettes, [&Footprints](std::span<const FSpritePart> Parts)
				{
					for (const FSpritePart& Part : Parts)
					{
						Footprints.push_back(SpritePartCell(Part));
					}
				});
			}
			const std::vector<FTextureRegion> FileSheets = BoundTextureRegions(Footprints);
			Sheets.insert(Sheets.end(), FileSheets.begin(), FileSheets.end());
		}
		return Sheets;
	}

	std::vector<FTextureRegion> FieldPanoramaTextureRegions(const FFieldMap& Map)
	{
		std::vector<FTextureRegion> Regions;
		const TResult<FEventScripts> Scripts = FEventScripts::Parse(Map.EventScripts);
		if (!Scripts)
		{
			return Regions;
		}
		// FE 80 and FE 82: the two-byte opcode, then FE 80's plain words.
		constexpr uint8 Extended = 0xFE;
		constexpr uint8 Params = 0x80;
		constexpr uint8 Colours = 0x82;
		const uint8* Code = Scripts->Code();
		const auto Word = [Code](std::size_t At) { return static_cast<int16>(Code[At] | Code[At + 1] << 8); };
		std::vector<FFieldPanorama> Panoramas;
		bool bMade = false;
		for (const uint16 At : FollowEventThreads(*Scripts).Reached)
		{
			if (Code[At] != Extended)
			{
				continue;
			}
			bMade = bMade || Code[At + 1] == Colours;
			if (Code[At + 1] != Params)
			{
				continue;
			}
			FFieldPanorama& Panorama = Panoramas.emplace_back();
			Panorama.VramX = Word(At + 2);
			Panorama.VramY = Word(At + 4);
			Panorama.Width = Word(At + 6) == 0 ? int16{1} : Word(At + 6);
			Panorama.Height = Word(At + 8);
			Panorama.ClutY = Word(At + 10);
			Panorama.TexturePage = Word(At + 12);
		}
		for (const FFieldPanorama& Panorama : bMade ? Panoramas : std::vector<FFieldPanorama>{})
		{
			const std::vector<FTextureRegion> Strip = PanoramaTextureRegions(Panorama);
			Regions.insert(Regions.end(), Strip.begin(), Strip.end());
		}
		std::sort(Regions.begin(), Regions.end());
		Regions.erase(std::unique(Regions.begin(), Regions.end()), Regions.end());
		return Regions;
	}
}
