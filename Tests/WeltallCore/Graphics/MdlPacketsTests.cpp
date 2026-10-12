#include "Graphics/GteVectors.h"
#include "Graphics/MdlLights.h"
#include "Graphics/MdlPackets.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	/** A part of four vertices in the z = 0 plane, normals toward -z. */
	FMdlPart Square()
	{
		FMdlPart Part;
		Part.Vertices = {{0, 0, 0}, {100, 0, 0}, {0, 100, 0}, {100, 100, 0}};
		Part.Normals = {{0, 0, -4096}, {0, 0, -4096}, {0, 0, -4096}, {0, 0, -4096}};
		return Part;
	}

	FMdlPrimitive Primitive(EMdlPrimitiveType Type, uint8 Code, FMdlColor Color = {100, 50, 25})
	{
		FMdlPrimitive Result;
		Result.Type = Type;
		Result.Code = Code;
		Result.Color = Color;
		Result.Vertices = {0, 1, 2, 3};
		return Result;
	}

	/** One white light shining along +z (so onto normals toward -z), and a dim ambient light. */
	FGte Lighting()
	{
		FMdlLights Lights;
		Lights.SetDirectional(0, {0, 0, 4096}, {4096, 4096, 4096});
		Lights.SetAmbient({16 * 16, 16 * 16, 16 * 16});
		FGte Gte;
		Lights.Load(Gte, FGteMatrix::Identity());
		return Gte;
	}
}

TEST_CASE("The packet builder keeps the static kinds' stored colors", "[Graphics]")
{
	FMdlPart Part = Square();
	Part.Primitives = {Primitive(EMdlPrimitiveType::StaticFlatQuad, 0x28), Primitive(EMdlPrimitiveType::StaticTexturedQuad, 0x2D),
		Primitive(EMdlPrimitiveType::StaticTexturedTriangle, 0x25)};
	const auto Packets = BuildMdlPackets(Part, 0, Lighting());
	REQUIRE(Packets);
	CHECK((*Packets)[0].Colors[0] == FMdlColor{100, 50, 25});
	CHECK((*Packets)[1].Colors[0] == FMdlColor{100, 50, 25});
	CHECK((*Packets)[1].Code == 0x2D);
	CHECK((*Packets)[2].Relight == EMdlRelight::None);
}

TEST_CASE("The packet builder lights the lightable kinds with a light mode and the Gouraud kinds always", "[Graphics]")
{
	FMdlPart Part = Square();
	Part.Primitives = {Primitive(EMdlPrimitiveType::FlatTriangle, 0x20), Primitive(EMdlPrimitiveType::TexturedTriangle, 0x24),
		Primitive(EMdlPrimitiveType::StaticGouraudTexturedTriangle, 0x34), Primitive(EMdlPrimitiveType::GouraudQuad, 0x38)};
	const FGte Gte = Lighting();
	const auto Unlit = BuildMdlPackets(Part, 0, Gte);
	REQUIRE_FALSE(Unlit);
	Part.Primitives[1].Code = 0x25;
	const auto Raw = BuildMdlPackets(Part, 0, Gte);
	REQUIRE(Raw);
	CHECK((*Raw)[0].Colors[0] == FMdlColor{100, 50, 25});

	const auto Lit = BuildMdlPackets(Part, 2, Gte);
	REQUIRE(Lit);
	// The face normal of (0, 0), (100, 0), (0, 100) points toward -z, into the light: full light
	// plus the ambient, which saturates the textured kinds and scales the stored color.
	CHECK((*Lit)[0].FaceNormal == TriangleNormal(Part.Vertices[0], Part.Vertices[1], Part.Vertices[2]));
	CHECK((*Lit)[0].Relight == EMdlRelight::FaceTimesColor);
	const FGteColor Face = Gte.NormalColorColor((*Lit)[0].FaceNormal, {100, 50, 25, 0x20});
	CHECK((*Lit)[0].Colors[0] == FMdlColor{Face.R, Face.G, Face.B});
	CHECK((*Lit)[1].Relight == EMdlRelight::Face);
	CHECK((*Lit)[1].Colors[0] == FMdlColor{255, 255, 255});
	CHECK((*Lit)[2].Relight == EMdlRelight::None);
	CHECK((*Lit)[2].Colors[2] == FMdlColor{255, 255, 255});
	CHECK((*Lit)[3].Colors[3] == (*Lit)[3].Colors[0]);
}

TEST_CASE("Relighting a packet follows the GTE's current lights", "[Graphics]")
{
	FMdlPart Part = Square();
	Part.Primitives = {Primitive(EMdlPrimitiveType::TexturedTriangle, 0x24)};
	auto Packets = BuildMdlPackets(Part, 2, Lighting());
	REQUIRE(Packets);
	// Turned half a turn about y, the light shines onto the back of the face: only the ambient is left.
	FMdlLights Lights;
	Lights.SetDirectional(0, {0, 0, 4096}, {4096, 4096, 4096});
	Lights.SetAmbient({16 * 16, 16 * 16, 16 * 16});
	FGte Turned;
	Lights.Load(Turned, RotationY(2048));
	RelightMdlPacket((*Packets)[0], Part.Primitives[0], Part, Turned);
	CHECK((*Packets)[0].Colors[0] == FMdlColor{16, 16, 16});
	CHECK(RelightReadsLightBuffer(EMdlRelight::Face));
	CHECK_FALSE(RelightReadsLightBuffer(EMdlRelight::Corners));
}

TEST_CASE("The packet builder refuses what it leaves undefined", "[Graphics]")
{
	FMdlPart Part = Square();
	Part.Primitives = {Primitive(EMdlPrimitiveType::StaticTexturedTriangle, 0x24)};
	CHECK_FALSE(BuildMdlPackets(Part, 0, Lighting()));
}

TEST_CASE("The packet builder builds an environment-mapped triangle raw, its texels left to the drawing", "[Graphics]")
{
	FMdlPart Part = Square();
	Part.Primitives = {Primitive(EMdlPrimitiveType::EnvironmentMappedTriangle, 0x25)};
	const TResult<std::vector<FMdlPacket>> Packets = BuildMdlPackets(Part, 2, Lighting());
	REQUIRE(Packets);
	CHECK((*Packets)[0].Code == 0x25);
	CHECK((*Packets)[0].Relight == EMdlRelight::None);
}
