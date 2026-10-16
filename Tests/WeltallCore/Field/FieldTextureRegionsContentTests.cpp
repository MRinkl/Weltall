#include "Field/FieldBackdrop.h"
#include "Field/FieldMap.h"
#include "Field/FieldPlay.h"
#include "Field/FieldPresenter.h"
#include "Field/FieldScene.h"
#include "Field/FieldTextureRegions.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/FieldPlaySession.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <set>

using namespace Weltall;

namespace
{
	struct FMapTextures
	{
		FFieldMap Map;
		std::vector<FTextureRegion> Regions;
	};

	FMapTextures LoadMapTextures(const FIso9660& Disc, int32 Number)
	{
		auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		auto Map = LoadFieldMap(Disc.Image(), *Index, Number);
		REQUIRE(Map);
		auto Scene = BuildFieldScene(*Map);
		REQUIRE(Scene);
		return {std::move(*Map), FieldSceneryTextureRegions(*Scene)};
	}
}

TEST_CASE("Lahan's scenery is drawn from thirty pictures that keep their keys in action mode", "[Content][Field][TextureRegion]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMapTextures Lahan = LoadMapTextures(*Disc, 2);
	REQUIRE(Lahan.Regions.size() == 30);
	FVram Plain;
	UploadFieldTextures(Lahan.Map, Plain, false);
	FVram Brightened;
	UploadFieldTextures(Lahan.Map, Brightened, true);
	std::set<uint64> Keys;
	for (const FTextureRegion& Region : Lahan.Regions)
	{
		CHECK(Region.U + Region.Width <= 256);
		CHECK(Region.V + Region.Height <= 256);
		const uint64 Key = TextureRegionKey(Plain, Region);
		CHECK(TextureRegionKey(Brightened, Region) == Key);
		Keys.insert(Key);
	}
	CHECK(Keys.size() == Lahan.Regions.size());
	// The village's sky: one 8-bit picture across the bottom of its page.
	CHECK(std::find_if(Lahan.Regions.begin(), Lahan.Regions.end(), [](const FTextureRegion& Region) { return Region.Width == 252 && Region.Height == 128 && Region.V == 128; })
		!= Lahan.Regions.end());
}

TEST_CASE("A map's panorama is one picture the texture export keys as the map draws it", "[Content][Field][TextureRegion]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// The mountain path's night sky, which its init script makes; five other maps make one.
	constexpr int32 MountainPathMap = 16;
	const FMapTextures Loaded = LoadMapTextures(*Disc, MountainPathMap);
	const std::vector<FTextureRegion> Panoramas = FieldPanoramaTextureRegions(Loaded.Map);
	REQUIRE_FALSE(Panoramas.empty());
	FVram Exported;
	UploadFieldTextures(Loaded.Map, Exported, false);

	auto Session = Testing::EnterFieldMap(*Disc, MountainPathMap);
	for (int32 Tick = 0; Tick < 200 && !Session->Frame.Backdrop.bShown; ++Tick)
	{
		Session->Run(1);
	}
	const FFieldBackdrop& Backdrop = Session->Frame.Backdrop;
	REQUIRE(Backdrop.bShown);
	const std::vector<FTextureRegion> Drawn = PanoramaTextureRegions(Backdrop.Panorama);
	REQUIRE_FALSE(Drawn.empty());
	// Every quad of the strip lies in one of its regions; each region is one the export finds, with the export's key.
	std::vector<FFieldBackdropQuad> Quads;
	DrawFieldBackdrop(Backdrop, 0, FieldBackdropWidth, Quads);
	int32 Textured = 0;
	int32 Covered = 0;
	for (const FFieldBackdropQuad& Quad : Quads)
	{
		if (Quad.Kind != FFieldBackdropQuad::EKind::Textured)
		{
			continue;
		}
		const uint32 View = PackTextureView(FTexturePage::FromAttribute(Quad.TexturePage), FClutPosition::FromAttribute(Quad.Clut));
		++Textured;
		Covered += std::any_of(Drawn.begin(), Drawn.end(), [&Quad, View](const FTextureRegion& Region)
		{
			return Region.View == View && Region.Contains(std::min(Quad.Texels[0][0], Quad.Texels[1][0]), Quad.Texels[0][1])
				&& Region.Contains(std::max(Quad.Texels[0][0], Quad.Texels[1][0]), Quad.Texels[2][1] - 1);
		}) ? 1 : 0;
	}
	CHECK(Textured > 0);
	CHECK(Covered == Textured);
	const FVram& Played = Session->Play->Drawing().Vram();
	for (const FTextureRegion& Region : Drawn)
	{
		CHECK(std::find(Panoramas.begin(), Panoramas.end(), Region) != Panoramas.end());
		CHECK(TextureRegionKey(Played, Region) == TextureRegionKey(Exported, Region));
	}
}
