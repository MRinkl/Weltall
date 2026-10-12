#include "Field/FieldSceneFrame.h"
#include "Field/FieldSceneJoins.h"
#include "Graphics/GteVectors.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <vector>

using namespace Weltall;

// Where the pieces of a map's scenery are drawn to meet: a piece set a hair off another is fitted to it, and nothing
// else moves.
namespace
{
	using FQuad = std::array<uint16, 4>;

	/** Models of flat quads, each placed once. A quad's corners go 0 1 / 2 3: its sides are 0-1, 1-3, 3-2 and 2-0. */
	struct FPieces
	{
		FFieldScene Scene;

		uint16 Add(const std::vector<FGteVector>& Vertices, const std::vector<FQuad>& Quads, const std::array<int32, 3>& Position = {}, int16 Yaw = 0)
		{
			FMdlPart& Part = Scene.Models.emplace_back().Parts.emplace_back();
			Part.Vertices = Vertices;
			FFieldSceneObject Object;
			Object.Model = static_cast<uint16>(Scene.Models.size() - 1);
			Object.Placement = RotationXyz({0, Yaw, 0});
			Object.Placement.T = Position;
			Object.Built = Object.Placement;
			Object.FirstPolygon = static_cast<uint32>(Scene.Polygons.size());
			Object.PolygonCount = static_cast<uint32>(Quads.size());
			for (const FQuad& Quad : Quads)
			{
				FMdlPrimitive Primitive;
				Primitive.Type = EMdlPrimitiveType::StaticFlatQuad;
				Primitive.Vertices = Quad;
				FFieldScenePolygon Polygon;
				Polygon.Object = static_cast<uint16>(Scene.Objects.size());
				Polygon.Primitive = static_cast<uint16>(Part.Primitives.size());
				Part.Primitives.push_back(Primitive);
				Scene.Polygons.push_back(Polygon);
			}
			Scene.Objects.push_back(Object);
			return static_cast<uint16>(Scene.Objects.size() - 1);
		}

		/** A wall about a doorway 76 wide and 116 high in the plane z = 0: the two jambs and the lintel. */
		uint16 AddWall(int16 Yaw = 0)
		{
			return Add({{-40, -116, 0}, {0, -116, 0}, {-40, 0, 0}, {0, 0, 0}, {76, -116, 0}, {116, -116, 0}, {76, 0, 0}, {116, 0, 0}, {-40, -160, 0}, {116, -160, 0}},
				{{0, 1, 2, 3}, {4, 5, 6, 7}, {8, 9, 0, 5}}, {}, Yaw);
		}

		/** The door of that doorway as one quad, set Position off it. */
		uint16 AddDoor(const std::array<int32, 3>& Position, int16 Yaw = 0)
		{
			return Add({{0, -116, 0}, {76, -116, 0}, {0, 0, 0}, {76, 0, 0}}, {{0, 1, 2, 3}}, Position, Yaw);
		}

		std::vector<FFieldSceneJoin> Joins() const { return BuildFieldSceneJoins(Scene); }
	};

	bool Is(const FFieldSceneJoin& Join, uint16 Object, uint16 Vertex, uint16 Anchor, const std::array<float, 3>& Point)
	{
		return Join.Object == Object && Join.Vertex == Vertex && Join.Anchor == Anchor && Join.Point == Point;
	}
}

TEST_CASE("A door set a hair off its frame is drawn at the frame's corners", "[Field]")
{
	// Turned as the maps turn their rooms, the door a unit off across and a unit off the wall's plane.
	for (const int16 Yaw : {int16{0}, int16{512}})
	{
		FPieces Pieces;
		const uint16 Wall = Pieces.AddWall(Yaw);
		const uint16 Door = Pieces.AddDoor({1, 0, 0}, Yaw);
		const std::vector<FFieldSceneJoin> Joins = Pieces.Joins();
		REQUIRE(Joins.size() == 4);
		CHECK(Is(Joins[0], Door, 0, Wall, {0.0f, -116.0f, 0.0f}));
		CHECK(Is(Joins[1], Door, 1, Wall, {76.0f, -116.0f, 0.0f}));
		CHECK(Is(Joins[2], Door, 2, Wall, {0.0f, 0.0f, 0.0f}));
		CHECK(Is(Joins[3], Door, 3, Wall, {76.0f, 0.0f, 0.0f}));
	}

	// The scene the field builds keeps them, found by object and vertex.
	FPieces Pieces;
	const uint16 Wall = Pieces.AddWall();
	const uint16 Door = Pieces.AddDoor({1, 0, 1});
	Pieces.Scene.Joins = Pieces.Joins();
	REQUIRE(Pieces.Scene.JoinOf(Door, 3) != nullptr);
	CHECK(Pieces.Scene.JoinOf(Door, 3)->Anchor == Wall);
	CHECK(Pieces.Scene.JoinOf(Wall, 3) == nullptr);
	CHECK(Pieces.Scene.JoinOf(Door, 4) == nullptr);
}

TEST_CASE("A corner beside the middle of another piece's edge is drawn on that edge", "[Field]")
{
	// A door of two quads, one above the other: the jambs' edges have no corner where the two meet.
	FPieces Pieces;
	const uint16 Wall = Pieces.AddWall();
	const uint16 Door = Pieces.Add({{0, -116, 0}, {76, -116, 0}, {0, -22, 0}, {76, -22, 0}, {0, 0, 0}, {76, 0, 0}}, {{0, 1, 2, 3}, {2, 3, 4, 5}}, {1, 0, 1});
	const std::vector<FFieldSceneJoin> Joins = Pieces.Joins();
	REQUIRE(Joins.size() == 6);
	CHECK(Is(Joins[0], Door, 0, Wall, {0.0f, -116.0f, 0.0f}));
	CHECK(Is(Joins[2], Door, 2, Wall, {0.0f, -22.0f, 0.0f}));
	CHECK(Is(Joins[3], Door, 3, Wall, {76.0f, -22.0f, 0.0f}));
	CHECK(Is(Joins[5], Door, 5, Wall, {76.0f, 0.0f, 0.0f}));
}

TEST_CASE("Pieces apart, one over the other, turned to the screen or moved by an animation keep their own places", "[Field]")
{
	{
		// Two units off: no hairline, a gap the map means.
		FPieces Pieces;
		Pieces.AddWall();
		Pieces.AddDoor({0, 0, 2});
		CHECK(Pieces.Joins().empty());
	}
	{
		// A poster over the foot of the left jamb, a unit in front of it: it lies to the same side of every edge it runs beside.
		FPieces Pieces;
		Pieces.AddWall();
		Pieces.Add({{-40, -100, 0}, {0, -100, 0}, {-40, 0, 0}, {0, 0, 0}}, {{0, 1, 2, 3}}, {0, 0, 1});
		CHECK(Pieces.Joins().empty());
	}
	{
		FPieces Pieces;
		Pieces.AddWall();
		const uint16 Door = Pieces.AddDoor({1, 0, 1});
		Pieces.Scene.Objects[Door].MatrixMode = EFieldMatrixMode::FacingScreen;
		CHECK(Pieces.Joins().empty());
	}
	{
		FPieces Pieces;
		Pieces.AddWall();
		const uint16 Door = Pieces.AddDoor({1, 0, 1});
		Pieces.Scene.Models[Pieces.Scene.Objects[Door].Model].Parts.front().Mime.emplace();
		CHECK(Pieces.Joins().empty());
	}
	{
		// Sharing their edge exactly under one placement, two pieces already meet.
		FPieces Pieces;
		Pieces.AddWall();
		Pieces.AddDoor({0, 0, 0});
		CHECK(Pieces.Joins().empty());
	}
}

TEST_CASE("A door drawn on both its sides is fitted as one", "[Field]")
{
	FPieces Pieces;
	const uint16 Wall = Pieces.AddWall();
	const uint16 Door = Pieces.Add({{0, -116, 0}, {76, -116, 0}, {0, 0, 0}, {76, 0, 0}}, {{0, 1, 2, 3}, {1, 0, 3, 2}}, {1, 0, 1});
	const std::vector<FFieldSceneJoin> Joins = Pieces.Joins();
	REQUIRE(Joins.size() == 4);
	CHECK(Is(Joins[0], Door, 0, Wall, {0.0f, -116.0f, 0.0f}));
	CHECK(Is(Joins[3], Door, 3, Wall, {76.0f, 0.0f, 0.0f}));
}

TEST_CASE("A model keeping one place under two vertex numbers moves both", "[Field]")
{
	// The door's two quads each keep their own copy of the corners where they meet.
	FPieces Pieces;
	const uint16 Wall = Pieces.AddWall();
	const uint16 Door = Pieces.Add({{0, -116, 0}, {76, -116, 0}, {0, -22, 0}, {76, -22, 0}, {0, -22, 0}, {76, -22, 0}, {0, 0, 0}, {76, 0, 0}},
		{{0, 1, 2, 3}, {4, 5, 6, 7}}, {1, 0, 1});
	const std::vector<FFieldSceneJoin> Joins = Pieces.Joins();
	REQUIRE(Joins.size() == 8);
	CHECK(Is(Joins[2], Door, 2, Wall, {0.0f, -22.0f, 0.0f}));
	CHECK(Is(Joins[4], Door, 4, Wall, {0.0f, -22.0f, 0.0f}));
}

TEST_CASE("A piece fitted to one that is itself fitted is drawn where that one ends", "[Field]")
{
	// On the ground (y = 0): a strip of three tiles, two tiles a unit behind it, and one tile a unit beside those.
	FPieces Pieces;
	const uint16 Strip = Pieces.Add({{0, 0, 0}, {10, 0, 0}, {20, 0, 0}, {30, 0, 0}, {0, 0, 10}, {10, 0, 10}, {20, 0, 10}, {30, 0, 10}},
		{{0, 1, 4, 5}, {1, 2, 5, 6}, {2, 3, 6, 7}});
	const uint16 Pair = Pieces.Add({{0, 0, 11}, {10, 0, 11}, {20, 0, 11}, {0, 0, 21}, {10, 0, 21}, {20, 0, 21}}, {{0, 1, 3, 4}, {1, 2, 4, 5}});
	const uint16 Tile = Pieces.Add({{-11, 0, 11}, {-1, 0, 11}, {-11, 0, 21}, {-1, 0, 21}}, {{0, 1, 2, 3}});
	const std::vector<FFieldSceneJoin> Joins = Pieces.Joins();
	REQUIRE(Joins.size() == 5);
	CHECK(Is(Joins[0], Pair, 0, Strip, {0.0f, 0.0f, 10.0f}));
	CHECK(Is(Joins[1], Pair, 1, Strip, {10.0f, 0.0f, 10.0f}));
	CHECK(Is(Joins[2], Pair, 2, Strip, {20.0f, 0.0f, 10.0f}));
	// The tile's near corner goes to the pair's, which the strip's draws; its far one to the pair's own.
	CHECK(Is(Joins[3], Tile, 1, Strip, {0.0f, 0.0f, 10.0f}));
	CHECK(Is(Joins[4], Tile, 3, Pair, {0.0f, 0.0f, 21.0f}));
}

TEST_CASE("A corner standing where a fitted corner of another piece stands follows it", "[Field]")
{
	// A rug lying before the door, touching it only at its corner: no edge of it runs beside any other.
	FPieces Pieces;
	const uint16 Wall = Pieces.AddWall();
	Pieces.AddDoor({1, 0, 1});
	const uint16 Rug = Pieces.Add({{0, 0, 0}, {10, 0, 10}, {-10, 0, 10}, {0, 0, 20}}, {{0, 1, 2, 3}}, {1, 0, 1});
	const std::vector<FFieldSceneJoin> Joins = Pieces.Joins();
	REQUIRE(Joins.size() == 5);
	CHECK(Is(Joins[4], Rug, 0, Wall, {0.0f, 0.0f, 0.0f}));
}

TEST_CASE("Pieces sharing an edge under different placements are drawn by one of them", "[Field]")
{
	// The same doorway, the door's model made five units to the side and placed five units back: where the two
	// meet in the world their vertices come through different matrices.
	FPieces Pieces;
	const uint16 Wall = Pieces.AddWall();
	const uint16 Door = Pieces.Add({{-5, -116, 0}, {71, -116, 0}, {-5, 0, 0}, {71, 0, 0}}, {{0, 1, 2, 3}}, {5, 0, 0});
	const std::vector<FFieldSceneJoin> Joins = Pieces.Joins();
	REQUIRE(Joins.size() == 4);
	CHECK(Is(Joins[0], Door, 0, Wall, {0.0f, -116.0f, 0.0f}));
	CHECK(Is(Joins[3], Door, 3, Wall, {76.0f, 0.0f, 0.0f}));
}

TEST_CASE("A frame says which models stand as the map's data places them", "[Field]")
{
	FPieces Pieces;
	const uint16 Wall = Pieces.AddWall();
	const uint16 Door = Pieces.AddDoor({1, 0, 1});
	for (FFieldSceneObject& Object : Pieces.Scene.Objects)
	{
		Object.Bounds = {{0, 0, 0}, 2000};
	}
	FFieldView View;
	View.View = FGteMatrix::Identity();
	View.View.T = {0, 0, 1000};
	View.Scene = View.View;
	View.ScreenRotation = FGteMatrix::Identity();
	View.Billboard = FGteMatrix::Identity();
	View.ProjectionDistance = 500;
	FFieldSceneFrame Frame;
	EvaluateFieldScene(Pieces.Scene, View, EFieldCulling::InRange, Frame);
	CHECK(Frame.Objects[Wall].bAsBuilt);
	CHECK(Frame.Objects[Door].bAsBuilt);

	// The door swung on its hinge, as a script opens it.
	FFieldSceneObject& Swung = Pieces.Scene.Objects[Door];
	Swung.Placement.M = RotationXyz({0, 0x20, 0}).M;
	EvaluateFieldScene(Pieces.Scene, View, EFieldCulling::InRange, Frame);
	CHECK(Frame.Objects[Wall].bAsBuilt);
	CHECK_FALSE(Frame.Objects[Door].bAsBuilt);
	Swung.Placement = Swung.Built;
	Swung.bWithoutWorldRotation = true;
	EvaluateFieldScene(Pieces.Scene, View, EFieldCulling::InRange, Frame);
	CHECK_FALSE(Frame.Objects[Door].bAsBuilt);
	Swung.bWithoutWorldRotation = false;
	const std::vector<FGteVector> Moved = Pieces.Scene.PartOf(Swung).Vertices;
	Swung.MovedVertices = Moved;
	EvaluateFieldScene(Pieces.Scene, View, EFieldCulling::InRange, Frame);
	CHECK_FALSE(Frame.Objects[Door].bAsBuilt);
	Swung.MovedVertices = {};
	// The game keeps a model's place as low halves: a place west of the origin reads as a large one.
	Swung.Built.T = {-27, -5, -4};
	Swung.Placement.T = {0x10000 - 27, 0x10000 - 5, 0x10000 - 4};
	EvaluateFieldScene(Pieces.Scene, View, EFieldCulling::InRange, Frame);
	CHECK(Frame.Objects[Door].bAsBuilt);
}
