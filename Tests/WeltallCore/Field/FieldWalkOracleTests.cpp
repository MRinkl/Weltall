#include "Field/FieldMap.h"
#include "Field/FieldWalkTrace.h"
#include "Graphics/GteMath.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"
#include "Support/OracleDigest.h"

#include <catch_amalgamated.hpp>

#include <string>

using namespace Weltall;
using Testing::DrawSigned;
using Testing::FOracleDigest;
using Testing::FXorShift32;

// The digests come from the game's own walkmesh routines (a trace, a trace over the ground, a move on the ground,
// a move in the air, a slide along an edge and the velocity from a direction) run under the oracle harness
// on the same maps' walkmeshes, over cases drawn from the same
// generator: actors near every layer's triangles, with random flags, floors and moves.

namespace
{
	constexpr int32 Cases = 256;
	constexpr int16 UnsetPoint = 0x5A5A;
	constexpr uint32 UnsetAttributes = 0xA5A5A5A5;

	int32 Fixed(int32 Whole, uint32 Fraction)
	{
		return static_cast<int32>(static_cast<uint32>(Whole) << 16 | (Fraction & 0xFFFF));
	}

	void AddEdge(FOracleDigest& Digest, const FWalkEdge& Edge)
	{
		Digest.I16(Edge.From[0], Edge.From[1], Edge.From[2], Edge.To[0], Edge.To[1], Edge.To[2]);
	}

	std::string RunMap(FFieldMap& Map, int32 Number, const FGteArctangents& Arctangents)
	{
		FEventContext Context;
		Context.Walkmesh = &Map.Walkmesh;
		Context.Arctangents = &Arctangents;
		const FFieldWalkmesh& Walkmesh = Map.Walkmesh;
		FXorShift32 Random(0x3A1C0000u + static_cast<uint32>(Number));
		FOracleDigest Digest;
		for (int32 Case = 0; Case < Cases; ++Case)
		{
			const uint32 LayerIndex = Random.Next() % Walkmesh.Layers.size();
			const FWalkmeshLayer& Layer = Walkmesh.Layers[LayerIndex];
			const uint32 TriangleIndex = Random.Next() % Layer.Triangles.size();
			const FWalkmeshTriangle& Triangle = Layer.Triangles[TriangleIndex];
			const FGteVector& Corner = Layer.Vertices[Triangle.Vertices[Random.Next() % 3]];
			FEventActorState State;
			const int32 WholeX = Corner[0] + DrawSigned(Random, 6);
			State.Position[0] = Fixed(WholeX, Random.Next());
			const int32 WholeZ = Corner[2] + DrawSigned(Random, 6);
			State.Position[2] = Fixed(WholeZ, Random.Next());
			const int32 WholeY = Corner[1] + DrawSigned(Random, 7);
			State.Position[1] = Fixed(WholeY, Random.Next());
			State.Flags0 = Random.Next();
			State.Flags1 = Random.Next() % 4 == 0 ? Random.Next() & 0x38 : 0;
			State.FloorAttributes = Walkmesh.Attributes[Triangle.Attribute];
			if (Random.Next() % 2 != 0)
			{
				State.FloorAttributes = Random.Next();
			}
			State.TargetHeight = static_cast<int16>(DrawSigned(Random, 16));
			Context.bPartyWalking = Random.Next() % 8 == 0;
			Context.Movement.FloorPushScale = static_cast<int16>(DrawSigned(Random, 16));
			std::array<int32, 3> Delta{DrawSigned(Random, 22), 0, 0};
			Delta[2] = DrawSigned(Random, 22);
			if (Random.Next() % 8 == 0)
			{
				Delta = {};
			}
			std::array<int32, 3> Far{DrawSigned(Random, 27), 0, 0};
			Far[2] = DrawSigned(Random, 27);
			const int32 Direction = static_cast<int16>(Random.Next());
			FWalkEdge Drawn;
			for (int16& Value : Drawn.From)
			{
				Value = static_cast<int16>(DrawSigned(Random, 12));
			}
			for (int16& Value : Drawn.To)
			{
				Value = static_cast<int16>(DrawSigned(Random, 12));
			}
			if (Random.Next() % 8 == 0)
			{
				Drawn = {};
			}
			const int32 Speed = std::array<int32, 4>{4, 8, 16, 32}[Random.Next() % 4];
			const int32 PushDirection = static_cast<int32>(Random.Next() & 0xFFFF);
			State.FloorTriangles = {-1, -1, -1, -1};
			State.FloorTriangles[LayerIndex] = static_cast<int16>(TriangleIndex);
			State.Layer = static_cast<uint16>(LayerIndex);

			for (const EWalkRule Rule : {EWalkRule::Air, EWalkRule::Ground})
			{
				for (const auto& [bProbe, Moved] : {std::pair{true, Delta}, std::pair{false, Delta}, std::pair{true, Far}, std::pair{false, Far}})
				{
					FWalkTrace Trace;
					Trace.Point = {UnsetPoint, UnsetPoint, UnsetPoint};
					Trace.Edge = Drawn;
					Trace.Attributes = UnsetAttributes;
					const bool bReached = TraceWalk(Context, State, Moved, Rule, bProbe, Trace);
					Digest.I32(bReached ? 0 : -1);
					Digest.I16(Trace.Point[0], Trace.Point[2]);
					AddEdge(Digest, Trace.Edge);
					if (bReached)
					{
						Digest.I16(Trace.Point[1]);
						if (Rule == EWalkRule::Ground)
						{
							Digest.I32(Trace.Attributes);
						}
					}
				}
			}
			for (const bool bGround : {true, false})
			{
				FEventActorState Moving = State;
				std::array<int32, 3> Moved = Delta;
				FWalkEdge Edge = Drawn;
				const bool bMoved = bGround ? MoveOnGround(Context, Moving, Moved, Direction, Edge) : MoveThroughAir(Context, Moving, Moved, Direction, Edge);
				Digest.I32(bMoved ? 0 : -1);
				Digest.I32(Moved[0], Moved[1], Moved[2]);
				Digest.I32(Moving.Flags0);
				Digest.I16(Moving.FloorHeight);
				AddEdge(Digest, Edge);
			}
			std::array<int32, 3> Slid = Delta;
			SlideAlongEdge(Context, Direction, Drawn, Slid);
			Digest.I32(Slid[0], Slid[1], Slid[2]);
			const std::array<int32, 3> Push = FloorPushVelocity(Context.Movement, Speed, PushDirection);
			Digest.I32(Push[0], Push[1], Push[2]);
		}
		return Digest.Hex();
	}
}

TEST_CASE("Walkmesh traces and moves match the game's", "[Field][Content][Oracle]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Arctangents = FGteArctangents::Read(*Executable);
	REQUIRE(Arctangents);
	// Pushes, slow ground, steep floors and ceilings; ladders and layer passes; slopes; clockwise
	// triangles; Lahan; a push over layers.
	const std::pair<int32, const char*> Digests[] = {
		{260, "7ba9df712383ec66149a9d4cd722205fe04309a9"},
		{58, "4f18971bec6d2f13ce4b169bb4d8adae5176a734"},
		{546, "c6530c19ac7d7c532571d82794940c23cb47ce9e"},
		{684, "676b3cdc14192b7347f765bf25df03c8cb2c3873"},
		{545, "13a59a4749511200716dd4d756b918ffe68be9e6"},
		{13, "fdbd4059d7b46c5564b61e5fa18625cfb0e9b6cf"},
		{1, "7da5a37b04a0e712a74fab1b1fcc7d05dfa33a7a"},
		{23, "c43025c58b3facedd9001fef7a80a3e2980e5435"},
	};
	for (const auto& [Number, Expected] : Digests)
	{
		INFO("map " << Number);
		auto Map = LoadFieldMap(Disc->Image(), *Index, Number);
		REQUIRE(Map);
		CHECK(RunMap(*Map, Number, *Arctangents) == Expected);
	}
}

