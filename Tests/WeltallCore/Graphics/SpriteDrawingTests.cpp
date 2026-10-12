#include "Graphics/Sprite.h"
#include "Graphics/SpriteDrawing.h"

#include <catch_amalgamated.hpp>

#include <vector>

using namespace Weltall;

namespace
{
	constexpr int32 Shift = 3;

	/** A sprite of one frame built by hand: its parts, at (0, 0, 0), scale 1, texel coordinates << 3. */
	FSprite MakeSprite(std::initializer_list<FSpritePart> Parts)
	{
		FSprite Sprite;
		Sprite.bHasTransform = true;
		Sprite.Scale = 0x1000;
		Sprite.BuildFlags = static_cast<uint32>(Parts.size()) << SpriteBuildFlags::PartCountShift | static_cast<uint32>(Shift) << SpriteBuildFlags::SizeShiftShift;
		std::size_t Index = 0;
		for (const FSpritePart& Part : Parts)
		{
			Sprite.Parts[Index++] = Part;
		}
		return Sprite;
	}

	FSpritePart Part(int16 X, int16 Y, uint8 Width, uint8 Height, uint32 Group = 4)
	{
		FSpritePart Result;
		Result.X = X;
		Result.Y = Y;
		Result.U = 32;
		Result.V = 64;
		Result.Width = Width;
		Result.Height = Height;
		Result.TexturePage = 0x1A;
		Result.Clut = 0x3810;
		Result.ColorCode = 0x2C808080;
		Result.Flags = Group;
		return Result;
	}

	/** The identity view, 1000 view units ahead of the eye. */
	FGteMatrix View()
	{
		FGteMatrix Result = FGteMatrix::Identity();
		Result.T = {0, 0, 1000};
		return Result;
	}

	std::array<float, 3> At(float X, float Y, float Z = 1000.0f)
	{
		return {X, Y, Z};
	}
}

TEST_CASE("A sprite's model matrix is its rotation, its rows scaled by its transform's scale and its columns by half its size factor, at its place", "[Graphics]")
{
	FSprite Sprite;
	// A quarter turn about y, the second row at half scale.
	Sprite.Rotation = {0, 0x400, 0};
	Sprite.TransformScale = {0x1000, 0x800, 0x1000};
	Sprite.Position = {0x12345678, -0x20000, 0x30000};
	FGteMatrix Matrix = SpriteModelMatrix(Sprite);
	CHECK(Matrix.M[0] == std::array<int16, 3>{0, 0, 0x1000});
	CHECK(Matrix.M[1] == std::array<int16, 3>{0, 0x800, 0});
	CHECK(Matrix.M[2] == std::array<int16, 3>{-0x1000, 0, 0});
	// The whole units of its place.
	CHECK(Matrix.T == std::array<int32, 3>{0x1234, -2, 3});
	// A size factor of 1000h scales each column by 800h.
	Sprite.SizeFactor = 0x1000;
	Matrix = SpriteModelMatrix(Sprite);
	CHECK(Matrix.M[0][2] == 0x800);
	CHECK(Matrix.M[1][1] == 0x400);
	CHECK(Matrix.M[2][0] == -0x800);
}

TEST_CASE("A sprite's parts are drawn at their texel offsets from its origin, less the pivot", "[Graphics]")
{
	FSprite Sprite = MakeSprite({Part(-8, -32, 16, 32)});
	Sprite.PivotX = 1;
	Sprite.PivotY = -2;
	Sprite.Position = {100 << 16, 50 << 16, 0};
	const FGteMatrix Matrix = SpriteDrawMatrix(Sprite, FGteMatrix::Identity(), View());
	// The pivot moves the matrix one way and the corners the other: at the sprite's own scale they cancel.
	CHECK(Matrix.T == std::array<int32, 3>{100 + 8, 50 - 16, 1000});
	std::vector<FSpriteQuad> Quads;
	DrawSpriteParts(Sprite, Matrix, 40, &Quads);
	REQUIRE(Quads.size() == 1);
	const FSpriteQuad& Quad = Quads[0];
	CHECK(Quad.ViewCorner(0) == At(100 - 64, 50 - 256));
	CHECK(Quad.ViewCorner(1) == At(100 + 64, 50 - 256));
	CHECK(Quad.ViewCorner(2) == At(100 - 64, 50));
	CHECK(Quad.ViewCorner(3) == At(100 + 64, 50));
	CHECK(Quad.U0 == 32);
	CHECK(Quad.U1 == 48);
	CHECK(Quad.V0 == 64);
	CHECK(Quad.V1 == 96);
	CHECK(Quad.OtIndex == 40);
	CHECK(Quad.TexturePage == 0x1A);
	CHECK(Quad.ColorCode == 0x2C808080);
}

TEST_CASE("A mirrored sprite draws its parts mirrored about its origin, their texels running the other way", "[Graphics]")
{
	FSprite Sprite = MakeSprite({Part(-8, -32, 16, 32)});
	Sprite.DrawFlags = SpriteDrawFlags::Mirrored;
	std::vector<FSpriteQuad> Quads;
	DrawSpriteParts(Sprite, SpriteDrawMatrix(Sprite, FGteMatrix::Identity(), View()), 0, &Quads);
	REQUIRE(Quads.size() == 1);
	// The texels' first column stays at corner 0, which now lies on the right.
	CHECK(Quads[0].ViewCorner(0) == At(64, -256));
	CHECK(Quads[0].ViewCorner(1) == At(-64, -256));

	FSprite Flipped = MakeSprite({Part(-8, -32, 16, 32)});
	Flipped.Parts[0].Flags |= 0x20;
	Quads.clear();
	DrawSpriteParts(Flipped, SpriteDrawMatrix(Flipped, FGteMatrix::Identity(), View()), 0, &Quads);
	CHECK(Quads[0].ViewCorner(0) == At(-64, 0));
	CHECK(Quads[0].ViewCorner(2) == At(-64, -256));
}

TEST_CASE("A sprite's hidden anchor groups draw nothing, and its groups can sort nearer", "[Graphics]")
{
	FSprite Sprite = MakeSprite({Part(0, 0, 8, 8, 4), Part(0, 0, 8, 8, 3), Part(0, 0, 8, 8, 2)});
	Sprite.DrawFlags = 0xEFu << SpriteDrawFlags::HiddenGroupsShift;
	std::vector<FSpriteQuad> Quads;
	DrawSpriteParts(Sprite, SpriteDrawMatrix(Sprite, FGteMatrix::Identity(), View()), 100, &Quads);
	REQUIRE(Quads.size() == 1);
	CHECK(Quads[0].OtIndex == 100);

	Sprite.DrawFlags = 0x8000000;
	Quads.clear();
	DrawSpriteParts(Sprite, SpriteDrawMatrix(Sprite, FGteMatrix::Identity(), View()), 100, &Quads);
	REQUIRE(Quads.size() == 3);
	CHECK(Quads[0].OtIndex == 96);
	CHECK(Quads[1].OtIndex == 97);
	CHECK(Quads[2].OtIndex == 98);
}

TEST_CASE("An anchor group's offset moves its parts at the sprite's scale, and the drawing leaves its matrix", "[Graphics]")
{
	FSprite Sprite = MakeSprite({Part(0, 0, 8, 8, 4), Part(0, 0, 8, 8, 2)});
	Sprite.Scale = 0x800;
	Sprite.bHasAnchors = true;
	Sprite.Anchors[2].OffsetX = 4;
	Sprite.Anchors[2].OffsetY = static_cast<uint8>(-2);
	const FGteMatrix Matrix = SpriteDrawMatrix(Sprite, FGteMatrix::Identity(), View());
	std::vector<FSpriteQuad> Quads;
	const FGteMatrix Left = DrawSpriteParts(Sprite, Matrix, 0, &Quads);
	REQUIRE(Quads.size() == 2);
	CHECK(Quads[0].ViewCorner(0) == At(0, 0));
	// 4 texels << 3 at half scale is 16 view units; -2 is -8.
	CHECK(Quads[1].ViewCorner(0) == At(16, -8));
	CHECK(Left.T == std::array<int32, 3>{16, -8, 1000});
}

TEST_CASE("A sprite's wading passes cut its parts at whole texels on either side of the line", "[Graphics]")
{
	const FSprite Sprite = MakeSprite({Part(-4, -20, 8, 20), Part(-4, -40, 8, 10)});
	const FGteMatrix Matrix = SpriteDrawMatrix(Sprite, FGteMatrix::Identity(), View());
	std::vector<FSpriteQuad> Above;
	DrawSpritePartsClipped(Sprite, Matrix, 7, ESpriteClip::KeepAbove, -6, Above);
	REQUIRE(Above.size() == 2);
	// The first part keeps its 14 rows above the line, texels V 64 to 78; the second lies wholly above.
	CHECK(Above[0].ViewCorner(0) == At(-32, -160));
	CHECK(Above[0].ViewCorner(2) == At(-32, -48));
	CHECK(Above[0].V0 == 64);
	CHECK(Above[0].V1 == 78);
	CHECK(Above[1].V1 == 74);
	CHECK(Above[0].OtIndex == 7);

	std::vector<FSpriteQuad> Below;
	DrawSpritePartsClipped(Sprite, Matrix, 7, ESpriteClip::KeepBelow, -6, Below);
	REQUIRE(Below.size() == 1);
	CHECK(Below[0].ViewCorner(0) == At(-32, -48));
	CHECK(Below[0].ViewCorner(2) == At(-32, 0));
	CHECK(Below[0].V0 == 78);
	CHECK(Below[0].V1 == 84);
}
