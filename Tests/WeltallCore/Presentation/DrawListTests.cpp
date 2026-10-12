#include "Presentation/DrawList.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

TEST_CASE("A rectangle's corners run left to right, then top to bottom", "[Presentation]")
{
	const FDrawPrimitive Rect = MakeGouraudRect({24, 136, 272, 84}, {FDrawColor{1, 2, 3}, FDrawColor{4, 5, 6}, FDrawColor{7, 8, 9}, FDrawColor{10, 11, 12}});
	CHECK(Rect.CornerCount == 4);
	CHECK(Rect.bGouraud);
	CHECK_FALSE(Rect.Texture.has_value());
	CHECK(Rect.Corners[0].X == 24.0f);
	CHECK(Rect.Corners[1].X == 296.0f);
	CHECK(Rect.Corners[2].Y == 220.0f);
	CHECK(Rect.Corners[3].Color == FDrawColor{10, 11, 12});
}

TEST_CASE("A sprite maps its texels onto pixels one to one, up to the page's last texel", "[Presentation]")
{
	const FDrawTexture Sheet{0, {704, 256, ETextureDepth::Indexed4, EBlendMode::Average}, {0, 480}};
	const FDrawPrimitive Sprite = MakeSprite(100, 50, 16, 8, Sheet, 240, 248);
	CHECK(Sprite.Texture == Sheet);
	CHECK(Sprite.Corners[0].U == 240.0f);
	CHECK(Sprite.Corners[0].V == 248.0f);
	// The right and bottom edges lie one past the last texel, which may be past 255.
	CHECK(Sprite.Corners[3].U == 256.0f);
	CHECK(Sprite.Corners[3].V == 256.0f);
	CHECK(Sprite.Corners[3].X == 116.0f);
	CHECK(Sprite.Corners[0].Color == FDrawColor{});
}

TEST_CASE("Primitives with the same blend key are paired across ticks", "[Presentation]")
{
	const auto Keyed = [](uint32 Key, uint8 Corners = 4)
	{
		FDrawPrimitive Primitive;
		Primitive.BlendKey = Key;
		Primitive.CornerCount = Corners;
		return Primitive;
	};
	FDrawList Earlier;
	Earlier.Primitives = {Keyed(7), Keyed(0), Keyed(3), Keyed(9, 3)};
	FDrawList Later;
	Later.Primitives = {Keyed(3), Keyed(0), Keyed(8), Keyed(7), Keyed(9)};
	CHECK(MatchBlendKeys(Earlier, Later) == std::vector<int32>{2, -1, -1, 0, -1});
	CHECK(MatchBlendKeys(FDrawList{}, Later) == std::vector<int32>{-1, -1, -1, -1, -1});
}
