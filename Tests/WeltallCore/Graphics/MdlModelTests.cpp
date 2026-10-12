#include "Graphics/MdlModel.h"
#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

#include <cstring>

using namespace Weltall;
using Testing::FByteWriter;

namespace
{
	struct FTestDelta
	{
		int16 X;
		int16 Y;
		int16 Z;
		uint16 Vertex;
	};

	/** One key's vertex deltas (and the same number of normal deltas when the part moves normals). */
	struct FTestKey
	{
		std::vector<FTestDelta> Vertices;
		std::vector<FTestDelta> Normals;
	};

	/** A one-part model laid out as the game stores it: header, part, vertices, normals, groups, source, MIMe. */
	struct FTestModel
	{
		uint16 Flags = 0;
		std::vector<FGteVector> Vertices;
		std::vector<FGteVector> Normals;
		FByteWriter Groups;
		uint16 GroupCount = 0;
		FByteWriter Source;
		std::vector<FTestKey> Keys;
		std::vector<uint16> Resets;
		bool bMime = false;
		uint32 ModelFlags = 0;

		FTestModel& Group(uint8 Type, std::initializer_list<std::array<uint16, 4>> Primitives)
		{
			Groups.U8(Type).U8(0x77).S16(static_cast<int16>(Primitives.size()));
			for (const auto& Indices : Primitives)
			{
				for (const uint16 Index : Indices)
				{
					Groups.U16(Index);
				}
			}
			++GroupCount;
			return *this;
		}

		static void Vectors(FByteWriter& Out, const std::vector<FGteVector>& List)
		{
			for (const FGteVector& Vector : List)
			{
				Out.S16(Vector[0]).S16(Vector[1]).S16(Vector[2]).S16(0);
			}
		}

		static void Deltas(FByteWriter& Out, const std::vector<FTestDelta>& List)
		{
			for (const FTestDelta& Delta : List)
			{
				Out.S16(Delta.X).S16(Delta.Y).S16(Delta.Z).U16(Delta.Vertex);
			}
		}

		std::vector<uint8> Bytes() const
		{
			const uint32 VerticesAt = 0x10 + 0x38;
			const uint32 AfterVertices = VerticesAt + static_cast<uint32>(Vertices.size() * 8);
			const uint32 NormalsAt = Normals.empty() ? VerticesAt : AfterVertices;
			const uint32 GroupsAt = AfterVertices + static_cast<uint32>(Normals.size() * 8);
			const uint32 SourceAt = GroupsAt + static_cast<uint32>(Groups.Size());
			const uint32 MimeAt = SourceAt + static_cast<uint32>(Source.Size());
			FByteWriter Out;
			Out.U32(1).U32(ModelFlags).U32(0).U32(0);
			Out.U16(Flags).U16(static_cast<uint16>(Vertices.size())).U16(0).U16(GroupCount);
			Out.U32(VerticesAt).U32(NormalsAt).U32(GroupsAt).U32(SourceAt).U32(0).U32(bMime ? MimeAt : 0);
			Out.S16(-1).S16(-2).S16(-3).S16(40).S16(4).S16(5).S16(6).S16(0).U32(0).U32(0);
			Vectors(Out, Vertices);
			Vectors(Out, Normals);
			Out.Bytes.insert(Out.Bytes.end(), Groups.Bytes.begin(), Groups.Bytes.end());
			Out.Bytes.insert(Out.Bytes.end(), Source.Bytes.begin(), Source.Bytes.end());
			if (bMime)
			{
				// The key table, then each key's vertex and normal deltas, then the reset list.
				uint32 DataAt = MimeAt + 4 + static_cast<uint32>((Keys.size() + 1) * 12);
				Out.U32(static_cast<uint32>(Keys.size()));
				for (const FTestKey& Key : Keys)
				{
					const uint32 NormalsOffset = DataAt + static_cast<uint32>(Key.Vertices.size() * 8);
					Out.U32(static_cast<uint32>(Key.Vertices.size())).U32(DataAt).U32(Key.Normals.empty() ? DataAt : NormalsOffset);
					DataAt = NormalsOffset + static_cast<uint32>(Key.Normals.size() * 8);
				}
				Out.U32(static_cast<uint32>(Resets.size())).U32(DataAt).U32(DataAt);
				for (const FTestKey& Key : Keys)
				{
					Deltas(Out, Key.Vertices);
					Deltas(Out, Key.Normals);
				}
				for (const uint16 Vertex : Resets)
				{
					Out.U16(Vertex);
				}
			}
			return Out.Bytes;
		}
	};

	FTestModel Square()
	{
		FTestModel Model;
		Model.Vertices = {{0, -58, 0}, {0, 0, 58}, {76, 58, 0}, {76, -58, 0}};
		return Model;
	}
}

TEST_CASE("MDL textured polygons take their page and CLUT from the commands before them", "[Graphics]")
{
	FTestModel Model = Square();
	Model.Group(13, {{1, 2, 0, 3}}).Group(5, {{0, 1, 2, 0}}).Group(12, {{0, 1, 2, 3}});
	Model.Source.U32(0xC4000095).U32(0xC8007BC0).U8s({0, 0, 0, 0x2D}).U16(0x0014).U16(0x0043).U16(0x3F14).U16(0x3F43);
	Model.Source.U32(0xC8007C00).U16(0x0201).U8(0xEE).U8(0x27).U16(0x0403).U16(0x0605);
	Model.Source.U8s({10, 20, 30, 0x2A});
	const auto Parsed = ParseMdlModel(Model.Bytes());
	REQUIRE(Parsed);
	REQUIRE(Parsed->Parts.size() == 1);
	const FMdlPart& Part = Parsed->Parts[0];
	CHECK(Part.Vertices[3] == FGteVector{76, -58, 0});
	CHECK(Part.BoundsMin == FGteVector{-1, -2, -3});
	CHECK(Part.BoundsMax == FGteVector{4, 5, 6});
	CHECK(Part.Normals.empty());
	CHECK_FALSE(Part.Mime.has_value());
	REQUIRE(Part.Primitives.size() == 3);

	const FMdlPrimitive& Quad = Part.Primitives[0];
	CHECK(Quad.Type == EMdlPrimitiveType::StaticTexturedQuad);
	CHECK(Quad.Vertices == std::array<uint16, 4>{1, 2, 0, 3});
	CHECK((Quad.TexturePage == 0x95 && Quad.Clut == 0x7BC0 && !Quad.bInheritsTexturePage && !Quad.bInheritsClut));
	CHECK(Quad.Uvs == std::array<FMdlUv, 4>{FMdlUv{0x14, 0}, FMdlUv{0x43, 0}, FMdlUv{0x14, 0x3F}, FMdlUv{0x43, 0x3F}});
	CHECK((Quad.Code == 0x2D && Quad.IsRawTexture() && !Quad.IsSemiTransparent()));
	CHECK(Quad.Traits().Corners == 4);

	// A textured triangle stores uv2 first and no colour; the CLUT command before it replaces only the CLUT.
	const FMdlPrimitive& Triangle = Part.Primitives[1];
	CHECK(Triangle.Type == EMdlPrimitiveType::StaticTexturedTriangle);
	CHECK((Triangle.TexturePage == 0x95 && Triangle.Clut == 0x7C00));
	CHECK(Triangle.Uvs == std::array<FMdlUv, 4>{FMdlUv{3, 4}, FMdlUv{5, 6}, FMdlUv{1, 2}, FMdlUv{}});
	CHECK(Triangle.Color == FMdlColor{});
	CHECK((Triangle.Code == 0x27 && Triangle.IsSemiTransparent()));

	const FMdlPrimitive& Flat = Part.Primitives[2];
	CHECK(Flat.Type == EMdlPrimitiveType::StaticFlatQuad);
	CHECK(Flat.Color == FMdlColor{10, 20, 30});
	CHECK(Flat.Code == 0x2A);
	CHECK_FALSE(Flat.Traits().bTextured);
	CHECK(Part.TextureStateAfter({1, 2}) == FMdlTextureState{0x95, 0x7C00});
}

TEST_CASE("MDL polygons before a part's first texture command use the builder's state", "[Graphics]")
{
	FTestModel Model = Square();
	Model.Group(1, {{0, 1, 2, 0}, {0, 2, 3, 0}});
	Model.Source.U16(0).U8(0).U8(0x24).U16(0).U16(0);
	Model.Source.U32(0xC4000088).U16(0).U8(0).U8(0x24).U16(0).U16(0);
	const auto Parsed = ParseMdlModel(Model.Bytes());
	REQUIRE(Parsed);
	const FMdlPart& Part = Parsed->Parts[0];
	const FMdlTextureState Before{0x11, 0x7B40};
	CHECK((Part.Primitives[0].bInheritsTexturePage && Part.Primitives[0].bInheritsClut));
	CHECK(Part.Primitives[0].Texture(Before) == Before);
	CHECK((!Part.Primitives[1].bInheritsTexturePage && Part.Primitives[1].bInheritsClut));
	CHECK(Part.Primitives[1].Texture(Before) == FMdlTextureState{0x88, 0x7B40});
	CHECK(Part.TextureStateAfter(Before) == FMdlTextureState{0x88, 0x7B40});
	CHECK(Part.Primitives[0].Traits().bLightable);

	FTestModel Untextured = Square();
	Untextured.Group(4, {{0, 1, 2, 0}});
	Untextured.Source.U8s({1, 2, 3, 0x20});
	const auto Flat = ParseMdlModel(Untextured.Bytes());
	REQUIRE(Flat);
	CHECK(Flat->Parts[0].TextureStateAfter(Before) == Before);
}

TEST_CASE("MDL Gouraud parts keep a normal per vertex and MIMe keys move both", "[Graphics]")
{
	FTestModel Model = Square();
	Model.Flags = 0x10;
	Model.Normals = {{0, -4096, 0}, {0, 0, 4096}, {4096, 0, 0}, {0, 4096, 0}};
	Model.Group(2, {{0, 1, 2, 0}});
	Model.Source.U8s({0x80, 0x40, 0x20, 0x30});
	Model.bMime = true;
	Model.Keys = {{{{1, -3, 8, 0}, {2, 0, 0, 3}}, {{0, 16, 0, 0}, {0, 0, -16, 3}}}};
	Model.Resets = {0, 3};
	const auto Parsed = ParseMdlModel(Model.Bytes());
	REQUIRE(Parsed);
	const FMdlPart& Part = Parsed->Parts[0];
	REQUIRE(Part.Normals.size() == 4);
	CHECK(Part.Normals[2] == FGteVector{4096, 0, 0});
	CHECK(Part.Primitives[0].Type == EMdlPrimitiveType::GouraudTriangle);
	CHECK(Part.Primitives[0].Color == FMdlColor{0x80, 0x40, 0x20});
	CHECK(Part.bMimeMovesNormals);
	REQUIRE(Part.Mime.has_value());
	REQUIRE(Part.Mime->Keys.size() == 1);
	const FMdlMimeKey& Key = Part.Mime->Keys[0];
	REQUIRE(Key.VertexDeltas.size() == 2);
	CHECK((Key.VertexDeltas[0].Delta == FGteVector{1, -3, 8} && Key.VertexDeltas[0].Vertex == 0));
	CHECK(Key.VertexDeltas[1].Vertex == 3);
	REQUIRE(Key.NormalDeltas.size() == 2);
	CHECK((Key.NormalDeltas[1].Delta == FGteVector{0, 0, -16} && Key.NormalDeltas[1].Vertex == 3));
	CHECK(Part.Mime->ResetVertices == std::vector<uint16>{0, 3});

	// Without the flag, the normal deltas are never read.
	Model.Flags = 0;
	const auto Still = ParseMdlModel(Model.Bytes());
	REQUIRE(Still);
	CHECK(Still->Parts[0].Mime->Keys[0].NormalDeltas.empty());
}

TEST_CASE("MDL models the kernel would misread are rejected", "[Graphics]")
{
	FTestModel OutOfRange = Square();
	OutOfRange.Group(4, {{0, 1, 4, 0}});
	OutOfRange.Source.U8s({0, 0, 0, 0x20});
	CHECK_FALSE(ParseMdlModel(OutOfRange.Bytes()));

	FTestModel UnknownType = Square();
	UnknownType.Group(17, {{0, 1, 2, 0}});
	UnknownType.Source.U8s({0, 0, 0, 0x20});
	CHECK_FALSE(ParseMdlModel(UnknownType.Bytes()));

	FTestModel NoNormals = Square();
	NoNormals.Group(2, {{0, 1, 2, 0}});
	NoNormals.Source.U8s({0, 0, 0, 0x30});
	CHECK_FALSE(ParseMdlModel(NoNormals.Bytes()));

	FTestModel ShortSource = Square();
	ShortSource.Group(13, {{0, 1, 2, 3}});
	ShortSource.Source.U32(0xC4000095).U8s({0, 0, 0, 0x2D});
	CHECK_FALSE(ParseMdlModel(ShortSource.Bytes()));

	FTestModel Relocated = Square();
	Relocated.ModelFlags = 1;
	CHECK_FALSE(ParseMdlModel(Relocated.Bytes()));

	FTestModel BadDelta = Square();
	BadDelta.Group(4, {{0, 1, 2, 0}});
	BadDelta.Source.U8s({0, 0, 0, 0x20});
	BadDelta.bMime = true;
	BadDelta.Keys = {{{{1, 1, 1, 9}}, {}}};
	CHECK_FALSE(ParseMdlModel(BadDelta.Bytes()));
}

TEST_CASE("An MDL model list gives each model from its offset", "[Graphics]")
{
	FTestModel First = Square();
	FTestModel Second = Square();
	Second.Vertices.push_back({9, 9, 9});
	const std::vector<uint8> A = First.Bytes();
	const std::vector<uint8> B = Second.Bytes();
	FByteWriter List;
	List.U32(2).U32(12).U32(static_cast<uint32>(12 + A.size()));
	List.Bytes.insert(List.Bytes.end(), A.begin(), A.end());
	List.Bytes.insert(List.Bytes.end(), B.begin(), B.end());
	const auto Models = ParseMdlModelList(List.Bytes);
	REQUIRE(Models);
	REQUIRE(Models->size() == 2);
	CHECK((*Models)[0].Parts[0].Vertices.size() == 4);
	CHECK((*Models)[1].Parts[0].Vertices.size() == 5);
	CHECK((*Models)[1].Parts[0].Primitives.empty());

	List.Bytes[4] = 0xFF;
	List.Bytes[5] = 0xFF;
	CHECK_FALSE(ParseMdlModelList(List.Bytes));
}
