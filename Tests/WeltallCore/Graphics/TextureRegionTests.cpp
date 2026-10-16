#include "Graphics/TextureRegion.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	const FTexturePage Page4{640, 0, ETextureDepth::Indexed4};
	const FTexturePage Page8{640, 0, ETextureDepth::Indexed8};

	/** A 4-bit picture of 8 x 2 texels at a place in VRAM, with its 16 colours at a CLUT. */
	void PlacePicture(FVram& Vram, int16 X, int16 Y, const FClutPosition& Clut, uint16 FirstColor)
	{
		Vram.Upload({{X, Y, 2, 2}, {0x3210, 0x0054, 0x1111, 0x2222}});
		std::vector<uint16> Colors(16);
		for (std::size_t Index = 0; Index < Colors.size(); ++Index)
		{
			Colors[Index] = static_cast<uint16>(FirstColor + Index * 0x0421);
		}
		Colors[0] = 0;
		Vram.Upload({{static_cast<int16>(Clut.X), static_cast<int16>(Clut.Y), 16, 1}, Colors});
	}
}

TEST_CASE("Texture views pack as the shaders unpack them, the port's rows included", "[Graphics][TextureRegion]")
{
	const FTexturePage Port{960, 768, ETextureDepth::Indexed8};
	const FClutPosition Clut{240, 1000};
	const uint32 View = PackTextureView(Port, Clut);
	CHECK(View == (15u | 1u << 4 | 1u << 5 | 1u << 7 | 15u << 8 | 1000u << 14));
	CHECK(TextureViewPage(View) == FTexturePage{960, 768, ETextureDepth::Indexed8});
	CHECK(TextureViewClut(View) == Clut);
	FTexturePage Blended = Page4;
	Blended.Blend = EBlendMode::Add;
	CHECK(PackTextureView(Blended, {}) == PackTextureView(Page4, {}));
}

TEST_CASE("Texture footprints merge where they overlap or meet, view by view", "[Graphics][TextureRegion]")
{
	const uint32 View = PackTextureView(Page4, {0, 480});
	const uint32 Other = PackTextureView(Page4, {16, 480});
	const std::vector<FTextureRegion> Regions = MergeTextureRegions({
		TextureRegionBetween(View, 64, 0, 127, 63),
		TextureRegionBetween(View, 0, 0, 63, 31),
		TextureRegionBetween(View, 0, 200, 15, 210),
		TextureRegionBetween(Other, 0, 0, 63, 31),
		TextureRegionBetween(View, 140, 64, 150, 70),
		// Meets the first two only once they are one.
		TextureRegionBetween(View, 128, 64, 139, 64),
	});
	REQUIRE(Regions.size() == 3);
	CHECK(Regions[0] == FTextureRegion{View, 0, 0, 151, 71});
	CHECK(Regions[1] == FTextureRegion{View, 0, 200, 16, 11});
	CHECK(Regions[2] == FTextureRegion{Other, 0, 0, 64, 32});
	CHECK(MergeTextureRegions({TextureRegionBetween(View, 0, 0, 9, 9), TextureRegionBetween(View, 11, 0, 20, 9)}).size() == 2);
	CHECK(WholeTextureView(View) == FTextureRegion{View, 0, 0, 256, 256});
	CHECK(TextureRegionBetween(View, 10, 10, 5, 5) == FTextureRegion{View, 10, 10, 1, 1});
}

TEST_CASE("A texture region's key follows its picture, not where it lies", "[Graphics][TextureRegion]")
{
	FVram First;
	PlacePicture(First, 640, 0, {0, 480}, 0x0400);
	const uint64 Key = TextureRegionKey(First, {PackTextureView(Page4, {0, 480}), 0, 0, 8, 2});

	FVram Moved;
	PlacePicture(Moved, 704, 20, {32, 500}, 0x0400);
	Moved.Upload({{706, 20, 4, 4}, std::vector<uint16>(16, 0x7777)});
	CHECK(TextureRegionKey(Moved, {PackTextureView({704, 0, ETextureDepth::Indexed4}, {32, 500}), 0, 20, 8, 2}) == Key);

	// Unused CLUT entries and the field's brightening of every colour but 0 leave it as it is.
	Moved.Upload({{32 + 9, 500, 1, 1}, {0x1234}});
	std::vector<uint16> Brightened(16);
	for (int32 Index = 0; Index < 16; ++Index)
	{
		const uint16 Color = Moved.At(32 + Index, 500);
		Brightened[static_cast<std::size_t>(Index)] = Color != 0 ? static_cast<uint16>(Color | 0x0C63) : 0;
	}
	Moved.Upload({{32, 500, 16, 1}, Brightened});
	CHECK(TextureRegionKey(Moved, {PackTextureView({704, 0, ETextureDepth::Indexed4}, {32, 500}), 0, 20, 8, 2}) == Key);

	FVram Recolored;
	PlacePicture(Recolored, 640, 0, {0, 480}, 0x0400);
	Recolored.Upload({{3, 480, 1, 1}, {0x7C00}});
	CHECK(TextureRegionKey(Recolored, {PackTextureView(Page4, {0, 480}), 0, 0, 8, 2}) != Key);

	FVram Redrawn;
	PlacePicture(Redrawn, 640, 0, {0, 480}, 0x0400);
	Redrawn.Upload({{641, 1, 1, 1}, {0x2223}});
	CHECK(TextureRegionKey(Redrawn, {PackTextureView(Page4, {0, 480}), 0, 0, 8, 2}) != Key);
	CHECK(TextureRegionKey(First, {PackTextureView(Page8, {0, 480}), 0, 0, 4, 2}) != Key);
	CHECK(TextureRegionKey(First, {PackTextureView(Page4, {0, 480}), 0, 0, 8, 1}) != Key);
}

TEST_CASE("A direct texture region's key is its colours", "[Graphics][TextureRegion]")
{
	const FTexturePage Direct{0, 256, ETextureDepth::Direct15};
	FVram Vram;
	Vram.Upload({{10, 260, 2, 1}, {0x001F, 0x8000}});
	const uint64 Key = TextureRegionKey(Vram, {PackTextureView(Direct, {}), 10, 4, 2, 1});
	FVram Elsewhere;
	Elsewhere.Upload({{200, 300, 2, 1}, {0x001F, 0x8000}});
	CHECK(TextureRegionKey(Elsewhere, {PackTextureView(Direct, {16, 2}), 200, 44, 2, 1}) == Key);
	Elsewhere.Upload({{201, 300, 1, 1}, {0x0000}});
	CHECK(TextureRegionKey(Elsewhere, {PackTextureView(Direct, {}), 200, 44, 2, 1}) != Key);
}

TEST_CASE("A decoded texture region is opaque but for texel 0", "[Graphics][TextureRegion]")
{
	FVram Vram;
	Vram.Upload({{640, 0, 1, 1}, {0x0010}});
	Vram.Upload({{0, 480, 2, 1}, {0x0000, 0x801F}});
	const FImageRgba8 Image = DecodeTextureRegion(Vram, {PackTextureView(Page4, {0, 480}), 0, 0, 2, 1});
	REQUIRE(Image.Width == 2);
	REQUIRE(Image.Height == 1);
	CHECK(Image.Pixels == std::vector<uint8>{0, 0, 0, 0, 255, 0, 0, 255});
	std::array<uint16, 256> Colors{};
	CHECK(ReadTextureViewClut(Vram, PackTextureView(Page4, {0, 480}), Colors).size() == 16);
	CHECK(ReadTextureViewClut(Vram, PackTextureView(Page8, {0, 480}), Colors).size() == 256);
	CHECK(ReadTextureViewClut(Vram, PackTextureView({0, 0, ETextureDepth::Direct15}, {}), Colors).empty());
}
