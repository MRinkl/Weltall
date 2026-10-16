#include "Graphics/TexturePictures.h"

#include <catch_amalgamated.hpp>

#include <memory>

using namespace Weltall;

namespace
{
	FVramImage PictureImage(int16 X, int16 Y, int16 Width, int16 Height, uint8 Depth, std::shared_ptr<const std::vector<uint16>> Palette = nullptr)
	{
		FVramImage Image{{X, Y, Width, Height}, std::vector<uint16>(static_cast<std::size_t>(Width) * Height, 0x1111)};
		Image.Picture = FVramPictureForm{Depth, std::move(Palette)};
		return Image;
	}
}

TEST_CASE("VRAM keeps the pictures uploaded into it until something is uploaded over them", "[Graphics][TexturePictures]")
{
	FVram Vram;
	Vram.Upload(PictureImage(640, 0, 64, 64, 0));
	Vram.Upload(PictureImage(704, 0, 32, 32, 1));
	Vram.Upload({{0, 480, 16, 1}, std::vector<uint16>(16, 1)});
	REQUIRE(Vram.Pictures().size() == 2);
	CHECK(Vram.Pictures()[0].Rect == FVramRect{640, 0, 64, 64});
	CHECK(Vram.Pictures()[1].Form.Depth == 1);

	// A unit written over a picture ends it; one beside it does not.
	Vram.Upload({{736, 0, 1, 1}, {0}});
	REQUIRE(Vram.Pictures().size() == 2);
	Vram.Upload({{735, 31, 1, 1}, {0}});
	REQUIRE(Vram.Pictures().size() == 1);
	CHECK(Vram.Pictures()[0].Rect.X == 640);

	// So does an upload that wraps around onto it, and a picture uploaded over another takes its place.
	Vram.Upload({{1020, 10, 8, 1}, std::vector<uint16>(8, 0)});
	Vram.Upload(PictureImage(640, 0, 16, 16, 2));
	Vram.Upload(PictureImage(0, 0, 4, 4, 2));
	Vram.Upload({{1020, 2, 8, 1}, std::vector<uint16>(8, 0)});
	REQUIRE(Vram.Pictures().size() == 1);
	CHECK(Vram.Pictures()[0].Rect == FVramRect{640, 0, 16, 16});
}

TEST_CASE("A view's picture regions are its pictures of its depth as far as its page reaches them, a part standing for the whole", "[Graphics][TexturePictures]")
{
	const std::vector<FVramPicture> Pictures{
		{{640, 0, 16, 32}, {0, nullptr}},
		// Crosses from the page at 640 into the next.
		{{680, 240, 40, 40}, {0, nullptr}},
		{{640, 100, 8, 8}, {1, nullptr}},
		{{1000, 300, 24, 10}, {2, nullptr}},
		{{0, 300, 16, 10}, {2, nullptr}},
	};
	std::vector<FTextureRegion> Regions;
	const uint32 View = PackTextureView({640, 0, ETextureDepth::Indexed4}, {0, 480});
	AddPictureRegionsInView(Pictures, View, Regions);
	REQUIRE(Regions.size() == 2);
	CHECK(Regions[0] == FTextureRegion{View, 0, 0, 64, 32});
	CHECK(Regions[1] == FTextureRegion{View, 160, 240, 96, 16, 160, 240, 160, 40});

	Regions.clear();
	const uint32 Next = PackTextureView({704, 256, ETextureDepth::Indexed4}, {0, 480});
	AddPictureRegionsInView(Pictures, Next, Regions);
	REQUIRE(Regions.size() == 1);
	CHECK(Regions[0] == FTextureRegion{Next, 0, 0, 64, 24, -96, -16, 160, 40});

	// A direct page at VRAM's right edge reaches on into its left columns.
	Regions.clear();
	const uint32 Edge = PackTextureView({960, 256, ETextureDepth::Direct15}, {});
	AddPictureRegionsInView(Pictures, Edge, Regions);
	REQUIRE(Regions.size() == 2);
	CHECK(Regions[0] == FTextureRegion{Edge, 40, 44, 24, 10});
	CHECK(Regions[1] == FTextureRegion{Edge, 64, 44, 16, 10});

	Regions.clear();
	AddPictureRegionsInView(Pictures, PackTextureView({640, 0, ETextureDepth::Indexed4}, {0, 480}) | 1u << 7, Regions);
	CHECK(Regions.empty());
}

TEST_CASE("A picture's seed is its part in the page of its first column and row, standing for all of it, with its colours", "[Graphics][TexturePictures]")
{
	const auto Palette = std::make_shared<const std::vector<uint16>>(16, 0x7FFF);
	const std::vector<FVramPicture> Pictures{
		{{648, 0, 16, 32}, {0, Palette}},
		{{680, 240, 80, 40}, {0, Palette}},
		{{320, 0, 200, 8}, {1, nullptr}},
	};
	std::vector<FTexturePictureSeed> Seeds;
	AddPictureSeeds(Pictures, Seeds);
	REQUIRE(Seeds.size() == 3);
	const uint32 Page640 = PackTextureView({640, 0, ETextureDepth::Indexed4}, {});
	CHECK(Seeds[0].Region == FTextureRegion{Page640, 32, 0, 64, 32});
	CHECK(Seeds[0].Palette == Palette);
	CHECK(Seeds[1].Region == FTextureRegion{Page640, 160, 240, 96, 16, 160, 240, 320, 40});
	// An 8-bit page is 128 units across.
	CHECK(Seeds[2].Region == FTextureRegion{PackTextureView({320, 0, ETextureDepth::Indexed8}, {}), 0, 0, 256, 8, 0, 0, 400, 8});
	CHECK(TextureRegionUnits(Seeds[1].Region) == FVramRect{680, 240, 80, 40});
	CHECK(TextureRegionUnits(Seeds[2].Region) == FVramRect{320, 0, 200, 8});
}

TEST_CASE("Every part of a picture larger than a page has the whole picture's key and pixels", "[Graphics][TexturePictures]")
{
	// An 8-bit picture 320 texels across at (768, 256), as the field's dungeon maps lie, drawn in columns from the pages
	// at 768, 832 and 896: the middle page begins halfway through the first.
	FVram Vram;
	std::vector<uint16> Units(160 * 4);
	for (std::size_t Unit = 0; Unit < Units.size(); ++Unit)
	{
		Units[Unit] = static_cast<uint16>(Unit * 0x0203 + 0x0101);
	}
	FVramImage Image{{768, 256, 160, 4}, Units};
	std::vector<uint16> Colors(256);
	for (std::size_t Index = 1; Index < Colors.size(); ++Index)
	{
		Colors[Index] = static_cast<uint16>(Index * 0x81);
	}
	Image.Picture = FVramPictureForm{1, std::make_shared<const std::vector<uint16>>(Colors)};
	Vram.Upload(Image);
	Vram.Upload({{0, 480, 256, 1}, Colors});
	std::vector<FTextureRegion> Regions;
	for (const int32 X : {768, 832, 896})
	{
		AddPictureRegionsInView(Vram.Pictures(), PackTextureView({X, 256, ETextureDepth::Indexed8}, {0, 480}), Regions);
	}
	REQUIRE(Regions.size() == 3);
	CHECK(Regions[1] == FTextureRegion{Regions[1].View, 0, 0, 192, 4, -128, 0, 320, 4});
	std::vector<FTexturePictureSeed> Seeds;
	AddPictureSeeds(Vram.Pictures(), Seeds);
	REQUIRE(Seeds.size() == 1);
	const uint64 Whole = TextureRegionKeyWith(ReadTextureRegionTexels(Vram, Seeds[0].Region), Colors);
	const FImageRgba8 Pixels = DecodeTextureRegionWith(Vram, Seeds[0].Region, Colors);
	CHECK(Pixels.Width == 320);
	CHECK(Pixels.Height == 4);
	for (const FTextureRegion& Part : Regions)
	{
		CHECK(TextureRegionKey(Vram, Part) == Whole);
		CHECK(DecodeTextureRegion(Vram, Part).Pixels == Pixels.Pixels);
		CHECK(TextureRegionUnits(Part) == FVramRect{768, 256, 160, 4});
	}
}

TEST_CASE("A region's key through a picture's own colours is its key through a CLUT of those colours", "[Graphics][TexturePictures]")
{
	FVram Vram;
	Vram.Upload({{640, 0, 2, 2}, {0x3210, 0x0054, 0x1111, 0x2222}});
	std::vector<uint16> Colors(16);
	for (std::size_t Index = 1; Index < Colors.size(); ++Index)
	{
		Colors[Index] = static_cast<uint16>(0x0400 + Index * 0x0421);
	}
	Vram.Upload({{0, 480, 16, 1}, Colors});
	const FTextureRegion Region{PackTextureView({640, 0, ETextureDepth::Indexed4}, {0, 480}), 0, 0, 8, 2};
	const FTextureRegionTexels Texels = ReadTextureRegionTexels(Vram, Region);
	CHECK(TextureRegionKeyWith(Texels, Colors) == TextureRegionKey(Vram, Region));
	CHECK(DecodeTextureRegionWith(Vram, Region, Colors).Pixels == DecodeTextureRegion(Vram, Region).Pixels);
	std::vector<uint16> Other = Colors;
	Other[5] = 0x7C00;
	CHECK(TextureRegionKeyWith(Texels, Other) != TextureRegionKey(Vram, Region));
	CHECK(TextureRegionKeyWith(Texels, {}) == AssetKeyOfDigest(Texels.Digest));
}
