#include "Field/FieldWalkmesh.h"
#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;
using Testing::FByteWriter;

namespace
{
	/**
	 * Two layers of the same 100 x 100 square sloping up toward +x: triangle 0 where x > z, 1 where
	 * x < z, sharing the diagonal. The attribute table holds AttributeCount words, word 1 = 0x20.
	 */
	std::vector<uint8> TwoSquares(uint32 AttributeCount = 256, uint8 SecondAttribute = 1, int16 Neighbour = 1, uint32 LayerCount = 2)
	{
		FByteWriter Triangles;
		Triangles.S16(0).S16(2).S16(1).S16(Neighbour).S16(-1).S16(-1).U8(0).U8(0);
		Triangles.S16(0).S16(3).S16(2).S16(-1).S16(-1).S16(0).U8(SecondAttribute).U8(0xFB);
		// The field aligns each vertex table to a word.
		Triangles.U16(0);
		FByteWriter Vertices;
		Vertices.S16(0).S16(0).S16(0).S16(0).S16(100).S16(-50).S16(0).S16(0);
		Vertices.S16(100).S16(-50).S16(100).S16(0).S16(0).S16(0).S16(100).S16(0);
		const uint32 Layer0 = 0x18 + 8 * 2;
		const uint32 Layer1 = Layer0 + static_cast<uint32>(Triangles.Size() + Vertices.Size());
		const uint32 Table = Layer1 + static_cast<uint32>(Triangles.Size() + Vertices.Size());
		FByteWriter Section;
		Section.U32(LayerCount).U32(28).U32(28).U32(0).U32(0).U32(Table);
		Section.U32(Layer0).U32(Layer0 + 30).U32(Layer1).U32(Layer1 + 30);
		for (int32 Layer = 0; Layer < 2; ++Layer)
		{
			Section.Bytes.insert(Section.Bytes.end(), Triangles.Bytes.begin(), Triangles.Bytes.end());
			Section.Bytes.insert(Section.Bytes.end(), Vertices.Bytes.begin(), Vertices.Bytes.end());
		}
		for (uint32 Index = 0; Index < AttributeCount; ++Index)
		{
			Section.U32(Index == 1 ? 0x20 : 0);
		}
		return Section.Bytes;
	}
}

TEST_CASE("The walkmesh gives each layer's triangles, neighbours and attribute words", "[Field]")
{
	const auto Walkmesh = ParseFieldWalkmesh(TwoSquares());
	REQUIRE(Walkmesh);
	REQUIRE(Walkmesh->Layers.size() == 2);
	CHECK(&Walkmesh->CameraLayer() == &Walkmesh->Layers[1]);
	const FWalkmeshLayer& Layer = Walkmesh->Layers[0];
	REQUIRE(Layer.Triangles.size() == 2);
	REQUIRE(Layer.Vertices.size() == 4);
	CHECK(Layer.Vertices[2] == FGteVector{100, -50, 100});
	const FWalkmeshTriangle& First = Layer.Triangles[0];
	CHECK(First.Vertices == std::array<uint16, 3>{0, 2, 1});
	CHECK(First.Neighbours == std::array<int16, 3>{1, -1, -1});
	const FWalkmeshTriangle& Second = Layer.Triangles[1];
	CHECK(Second.Ceiling == -5);
	REQUIRE(Walkmesh->Attributes.size() == 256);
	CHECK(Walkmesh->Attributes[Second.Attribute] == 0x20);
}

TEST_CASE("A walkmesh whose attribute table is cut short keeps the words it has", "[Field]")
{
	const auto Walkmesh = ParseFieldWalkmesh(TwoSquares(160));
	REQUIRE(Walkmesh);
	CHECK(Walkmesh->Attributes.size() == 160);
	CHECK_FALSE(ParseFieldWalkmesh(TwoSquares(160, 160)));
}

TEST_CASE("Walkmeshes the field would misread are rejected", "[Field]")
{
	CHECK_FALSE(ParseFieldWalkmesh(TwoSquares(256, 1, 2)));
	CHECK_FALSE(ParseFieldWalkmesh(TwoSquares(256, 1, -2)));
	CHECK_FALSE(ParseFieldWalkmesh(TwoSquares(256, 1, 1, 0)));
	CHECK_FALSE(ParseFieldWalkmesh(TwoSquares(256, 1, 1, 5)));
	std::vector<uint8> Short = TwoSquares();
	Short.resize(0x18 + 8 + 28 + 4);
	CHECK_FALSE(ParseFieldWalkmesh(Short));
}
