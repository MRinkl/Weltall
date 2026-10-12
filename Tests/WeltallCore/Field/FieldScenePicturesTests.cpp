#include "Field/FieldScenePictures.h"

#include <catch_amalgamated.hpp>

#include <vector>

using namespace Weltall;

// A scene staged before a picture: what counts as one, and the picture marked for the drawing where the map stages one.
namespace
{
	/** Map 73's picture: actor 6's model. */
	constexpr int32 PictureMap = 73;
	constexpr uint16 PictureActor = 6;
	constexpr int32 Columns = 4;
	constexpr int32 Rows = 4;

	/** A picture Width x Height in the plane z = 0 from (Left, Top), quads of Columns x Rows, corners 0 1 / 2 3, each its own texels. */
	FMdlPart Picture(int16 Left = -360, int16 Top = 2, int16 Width = 720, int16 Height = 540)
	{
		FMdlPart Part;
		for (int32 Row = 0; Row <= Rows; ++Row)
		{
			for (int32 Column = 0; Column <= Columns; ++Column)
			{
				Part.Vertices.push_back({static_cast<int16>(Left + Width * Column / Columns), static_cast<int16>(Top + Height * Row / Rows), 0});
			}
		}
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			for (int32 Column = 0; Column < Columns; ++Column)
			{
				FMdlPrimitive Quad;
				Quad.Type = EMdlPrimitiveType::StaticTexturedQuad;
				const auto At = [Row, Column](int32 Down, int32 Across) { return static_cast<uint16>((Row + Down) * (Columns + 1) + Column + Across); };
				Quad.Vertices = {At(0, 0), At(0, 1), At(1, 0), At(1, 1)};
				const uint8 U = static_cast<uint8>(Column * 40), V = static_cast<uint8>(Row * 50);
				Quad.Uvs = {{{U, V}, {static_cast<uint8>(U + 39), V}, {U, static_cast<uint8>(V + 49)}, {static_cast<uint8>(U + 39), static_cast<uint8>(V + 49)}}};
				Part.Primitives.push_back(Quad);
			}
		}
		return Part;
	}

	/** The picture placed by actor 6, and the same model placed by actor 7. */
	FFieldScene SceneOf(const FMdlPart& Part)
	{
		FFieldScene Scene;
		Scene.Models.emplace_back().Parts.push_back(Part);
		for (const uint16 Actor : {PictureActor, static_cast<uint16>(PictureActor + 1)})
		{
			FFieldSceneObject Object;
			Object.Actor = Actor;
			Object.PolygonCount = static_cast<uint32>(Part.Primitives.size());
			Scene.Objects.push_back(Object);
		}
		return Scene;
	}
}

TEST_CASE("A scene's picture is textured quads tiling a flat 4:3 rectangle", "[Field]")
{
	CHECK(IsScenePicture(Picture()));
	CHECK(IsScenePicture(Picture(-720, -540, 1440, 1080)));
	// Not 4:3.
	CHECK_FALSE(IsScenePicture(Picture(-360, 0, 720, 480)));
	FMdlPart Bent = Picture();
	Bent.Vertices[12][2] = 1;
	CHECK_FALSE(IsScenePicture(Bent));
	FMdlPart Holed = Picture();
	Holed.Primitives.pop_back();
	CHECK_FALSE(IsScenePicture(Holed));
	FMdlPart Untextured = Picture();
	Untextured.Primitives[5].Type = EMdlPrimitiveType::StaticFlatQuad;
	CHECK_FALSE(IsScenePicture(Untextured));
}

TEST_CASE("A scene's picture is marked where the map stages one, and nothing else is", "[Field]")
{
	FFieldScene Scene = SceneOf(Picture());
	MarkScenePictures(PictureMap, Scene);
	CHECK(Scene.Objects[0].bPicture);
	// Another actor's model of the same form is no picture of the map's.
	CHECK_FALSE(Scene.Objects[1].bPicture);
	CHECK(Scene.Models[0].Parts.front().Vertices == Picture().Vertices);
	FFieldScene Elsewhere = SceneOf(Picture());
	MarkScenePictures(PictureMap - 1, Elsewhere);
	CHECK_FALSE(Elsewhere.Objects[0].bPicture);
	// On the map, a model of another form is left as it is.
	FFieldScene Other = SceneOf(Picture(-360, 0, 720, 480));
	MarkScenePictures(PictureMap, Other);
	CHECK_FALSE(Other.Objects[0].bPicture);
}
