#include "Field/EventPlacement.h"
#include "Field/FieldCollisions.h"
#include "Field/FieldMap.h"
#include "Field/FieldSpriteFiles.h"
#include "Field/FieldSprites.h"
#include "Game/PsxRandom.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/EventStateLayout.h"
#include "Support/GameDiscs.h"
#include "Support/OracleDigest.h"
#include "Support/SpriteDigest.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <string>

using namespace Weltall;
using Testing::DrawSigned;
using Testing::FOracleDigest;
using Testing::FXorShift32;

// The digests come from the original's collision of the controlled actor (with its platform and box tests and, last,
// the controlled actor's layer step) run under the oracle harness
// on real maps, with the actors' real sprites and the maps' models as platforms, over ticks that gather
// the other actors round the controlled one with random bodies, flags and platforms.

namespace
{
	constexpr uint32 LayoutSeed = 0x1D5C6A31;
	constexpr int32 Ticks = 64;
	constexpr std::array<uint32, 6> LeaderFlags0 = {0x800, 0x40000, 0x80, 0x10000, 0x1000, 0};
	constexpr uint32 RideRecorded = 0x80;
	constexpr std::pair<std::size_t, std::size_t> Masked[] = {{0x110, 0x11C}, {0x120, 0x124}, {0x68, 0x6E}};

	int32 Fixed(int32 Whole, uint32 Fraction)
	{
		return static_cast<int32>(static_cast<uint32>(Whole) << 16 | (Fraction & 0xFFFF));
	}

	int16 WholeOf(int32 Value)
	{
		return static_cast<int16>(static_cast<uint32>(Value) >> 16);
	}

	struct FFieldFiles
	{
		const FIso9660* Disc = nullptr;
		FGameFileIndex Index;
		FGteArctangents Arctangents;
	};

	/** The actor's sprite made as the field makes a map sprite for it, with the script state's books kept. */
	void GiveSprite(FEventContext& Context, FFieldSprites& Sprites, int32 Actor, int32 Slot)
	{
		FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
		FFieldActor& Record = Context.Actors[static_cast<std::size_t>(Actor)];
		State.ModelIndex = static_cast<uint8>(Slot);
		State.ModelId = static_cast<uint8>(0x80 | Slot);
		State.SpriteDraw &= ~0x1Fu;
		State.SpriteTexture &= ~0x30000000u;
		FFieldSpriteRequest Request;
		Request.Actor = Actor;
		Request.Slot = Slot;
		Request.Position = State.Position;
		Request.RecordHeight = static_cast<int16>(Record.Translation[1]);
		const auto Result = Sprites.LoadSprite(Request);
		REQUIRE(Result);
		State.Height = static_cast<uint16>(Result->FrameTop * 2);
		if (Result->bNoAnimations)
		{
			State.AnimationRequest = 0xFF;
			State.Flags1 |= EventActorFlags1::NoAnimations;
		}
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			Record.Translation[Axis] = WholeOf(State.Position[Axis]);
			Record.Position[Axis] = Record.Translation[Axis];
		}
	}

	std::string RunMap(const FFieldFiles& Files, int32 Number)
	{
		auto Map = LoadFieldMap(Files.Disc->Image(), Files.Index, Number);
		REQUIRE(Map);
		const auto Scripts = FEventScripts::Parse(Map->EventScripts);
		REQUIRE(Scripts);
		const auto SpriteFiles = ParseFieldSpriteFiles(Map->SpriteSets);
		REQUIRE(SpriteFiles);
		const FFieldWalkmesh& Walkmesh = Map->Walkmesh;
		FPsxRandom Random(LayoutSeed);
		FEventContext Context;
		Context.MapNumber = Number;
		Context.RunningMap = Number;
		Context.Walkmesh = &Map->Walkmesh;
		Context.MapModels = Map->Models;
		Context.Arctangents = &Files.Arctangents;
		Context.Random = &Random;
		Context.Actors = CreateFieldActors(Map->Actors);
		const int32 Count = Scripts->ActorCount();
		Context.States.resize(static_cast<std::size_t>(Count));
		for (std::size_t Actor = 0; Actor < Context.States.size(); ++Actor)
		{
			Context.States[Actor].Reset(Random.Next());
			REQUIRE(PlaceActorOnLoad(Context.States[Actor], Context.Actors[Actor], Map->Walkmesh));
		}
		Context.PartyActors = {0, FEventContext::NoActor, FEventContext::NoActor};
		const FPartySpriteFiles NoParty(Files.Disc->Image(), Files.Index);
		FFieldSprites Sprites(*SpriteFiles, Map->SpriteSlots, NoParty, Context.States, Files.Disc->Image(), Files.Index);
		Sprites.System().Random = &Random;
		Sprites.System().Arctangents = &Files.Arctangents;
		Context.Models = &Sprites;
		int32 Slot = 0;
		for (int32 Index = 1; Index < static_cast<int32>(SpriteFiles->size()); ++Index)
		{
			if ((*SpriteFiles)[static_cast<std::size_t>(Index)].AnimationCount((*SpriteFiles)[static_cast<std::size_t>(Index)].Tables()) > (*SpriteFiles)[static_cast<std::size_t>(Slot)].AnimationCount((*SpriteFiles)[static_cast<std::size_t>(Slot)].Tables()))
			{
				Slot = Index;
			}
		}
		for (int32 Actor = 0; Actor < Count; ++Actor)
		{
			GiveSprite(Context, Sprites, Actor, Slot);
		}

		FXorShift32 Draw(0xC0110000u + static_cast<uint32>(Number));
		for (int32 Actor = 0; Actor < Count; ++Actor)
		{
			Context.States[static_cast<std::size_t>(Actor)].SpriteDraw |= RideRecorded;
			FFieldActor& Record = Context.Actors[static_cast<std::size_t>(Actor)];
			Record.Model = static_cast<uint16>(Draw.Next() % Map->Models.size());
			Record.Flags &= static_cast<uint16>(~FieldActorFlag::NoModel);
		}
		const uint32 WalkingLayers = static_cast<uint32>(std::max<std::size_t>(Walkmesh.Layers.size() - 1, 1));
		int32 Resets = 0;
		FOracleDigest Digest;
		for (int32 Tick = 0; Tick < Ticks; ++Tick)
		{
			const int32 LeaderIndex = static_cast<int32>(Draw.Next() % static_cast<uint32>(Count));
			Context.LeaderActor = static_cast<uint8>(LeaderIndex);
			FGteVector Angles{};
			if (Draw.Next() % 4 == 0)
			{
				for (int16& Angle : Angles)
				{
					Angle = static_cast<int16>(DrawSigned(Draw, 12));
				}
			}
			Context.Camera.WorldAngles = Angles;
			Context.Camera.WorldRotation = RotationXyz(Angles);
			Context.bPartyWalking = Draw.Next() % 8 == 0;
			Context.Movement.bFirstTickDone = Draw.Next() % 4 != 0;

			FEventActorState& Leader = Context.States[static_cast<std::size_t>(LeaderIndex)];
			FSprite& LeaderSprite = *Sprites.ActorSprite(LeaderIndex);
			const uint32 LayerIndex = Draw.Next() % WalkingLayers;
			const FWalkmeshLayer& Layer = Walkmesh.Layers[LayerIndex];
			const uint32 TriangleIndex = Draw.Next() % Layer.Triangles.size();
			const FGteVector& Corner = Layer.Vertices[Layer.Triangles[TriangleIndex].Vertices[Draw.Next() % 3]];
			const int32 X = Corner[0] + DrawSigned(Draw, 5);
			Leader.Position[0] = Fixed(X, Draw.Next());
			const int32 Z = Corner[2] + DrawSigned(Draw, 5);
			Leader.Position[2] = Fixed(Z, Draw.Next());
			const int32 Y = Corner[1] + DrawSigned(Draw, 6);
			Leader.Position[1] = Fixed(Y, Draw.Next());
			Leader.FloorTriangles[LayerIndex] = static_cast<int16>(TriangleIndex);
			Leader.Layer = static_cast<uint16>(LayerIndex);
			for (int32& Component : Leader.Step)
			{
				Component = DrawSigned(Draw, 21);
			}
			for (int32& Component : Leader.Velocity)
			{
				Component = DrawSigned(Draw, 16);
			}
			Leader.Flags0 ^= LeaderFlags0[Draw.Next() % LeaderFlags0.size()];
			if (Draw.Next() % 8 == 0)
			{
				Leader.Flags1 ^= 0x200000;
			}
			const uint32 Radius = 8 + Draw.Next() % 32;
			Leader.Height = static_cast<uint16>(0x20 + Draw.Next() % 0x60);
			Leader.Radius = static_cast<uint16>(Radius);
			uint8 Ridden = static_cast<uint8>(Draw.Next() % static_cast<uint32>(Count));
			if (Draw.Next() % 3 != 0)
			{
				Ridden = FEventContext::NoActor;
			}
			Leader.LinkedActor = Ridden;
			Leader.FloorAttributes = Draw.Next() % 4 == 0 ? 0x400000u : 0u;
			LeaderSprite.Extra.Signal = static_cast<uint16>(Draw.Next() % 4 == 0 ? 1 : 0);
			const std::array<int32, 3> Whole{WholeOf(Leader.Position[0]), WholeOf(Leader.Position[1]), WholeOf(Leader.Position[2])};
			for (int32 Index = 0; Index < Count; ++Index)
			{
				if (Index == LeaderIndex)
				{
					continue;
				}
				FEventActorState& Other = Context.States[static_cast<std::size_t>(Index)];
				FFieldActor& Record = Context.Actors[static_cast<std::size_t>(Index)];
				const int32 OtherX = Whole[0] + DrawSigned(Draw, 7);
				Other.Position[0] = Fixed(OtherX, Draw.Next());
				const int32 OtherZ = Whole[2] + DrawSigned(Draw, 7);
				Other.Position[2] = Fixed(OtherZ, Draw.Next());
				const int32 OtherY = Whole[1] + DrawSigned(Draw, 6);
				Other.Position[1] = Fixed(OtherY, Draw.Next());
				for (int32& Component : Other.Step)
				{
					Component = DrawSigned(Draw, 18);
				}
				for (int32& Component : Other.Velocity)
				{
					Component = DrawSigned(Draw, 16);
				}
				uint32 Flags0 = Other.Flags0 & ~(0x1u | 0x10u | 0x80u | 0x2000u);
				Flags0 |= Draw.Next() % 8 == 0 ? 0x1u : 0u;
				Flags0 |= Draw.Next() % 2 != 0 ? 0x10u : 0u;
				Flags0 |= Draw.Next() % 8 == 0 ? 0x80u : 0u;
				Flags0 |= Draw.Next() % 4 == 0 ? 0x2000u : 0u;
				uint32 Flags1 = Other.Flags1 & ~(0x80u | 0x40u | 0x100u | 0x4000u | 0x8000u | 0x400000u | 0x800000u);
				if (Draw.Next() % 4 == 0)
				{
					Flags1 |= 0x80u | (Draw.Next() % 2 != 0 ? 0x40u : 0u);
				}
				Flags1 |= Draw.Next() % 4 == 0 ? 0x800000u : 0u;
				Other.Flags0 = Flags0;
				Other.Flags1 = Flags1;
				Other.Radius = static_cast<uint16>(8 + Draw.Next() % 32);
				Other.Height = static_cast<uint16>(0x10 + Draw.Next() % 0x80);
				Other.HalfWidth = static_cast<uint16>(8 + Draw.Next() % 32);
				Other.HalfDepth = static_cast<uint16>(8 + Draw.Next() % 32);
				Other.PushCount = static_cast<uint8>(Draw.Next() & 0x3F);
				const int32 TranslationX = Whole[0] + DrawSigned(Draw, 8);
				const int32 TranslationY = Whole[1] + DrawSigned(Draw, 7);
				const int32 TranslationZ = Whole[2] + DrawSigned(Draw, 8);
				Record.Translation = {TranslationX, TranslationY, TranslationZ};
				const int16 Yaw = Draw.Next() % 2 != 0 ? static_cast<int16>(Draw.Next() & 0xFFF) : int16{0};
				Record.Rotation = {0, Yaw, 0};
				Record.Matrix = RotationXyz(Record.Rotation).M;
				const uint32 Axis = Draw.Next() % 4;
				Other.Status = (Other.Status & ~3u) | Axis;
				Other.ExtraAngle = static_cast<int16>(Draw.Next() & 0xFFF);
			}
			if (const auto Error = CollideControlledActor(Context))
			{
				FAIL(Error->Message);
			}
			if (Context.bPadInputDropped)
			{
				++Resets;
				Context.bPadInputDropped = false;
			}
			Digest.I32(Resets);
			for (int32 Actor = 0; Actor < Count; ++Actor)
			{
				const FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
				auto Bytes = Testing::ToOriginalLayout(State);
				for (const auto& [Start, End] : Masked)
				{
					std::fill(Bytes.begin() + static_cast<std::ptrdiff_t>(Start), Bytes.begin() + static_cast<std::ptrdiff_t>(End), uint8{0});
				}
				for (const uint8 Byte : Bytes)
				{
					Digest.U8(Byte);
				}
				Digest.I16(State.Ride.PlatformRotation[0], State.Ride.PlatformRotation[1], State.Ride.PlatformRotation[2], State.Ride.Distance);
				const FFieldActor& Record = Context.Actors[static_cast<std::size_t>(Actor)];
				Digest.I16(Record.Flags);
				Digest.I32(Record.Translation[1]);
				Testing::DigestSprite(Digest, *Sprites.SpriteOf(Actor));
			}
		}
		return Digest.Hex();
	}
}

TEST_CASE("The controlled actor meets the others as the game's collisions have it", "[Field][Content][Oracle]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	auto Arctangents = FGteArctangents::Read(*Executable);
	REQUIRE(Arctangents);
	const FFieldFiles Files{Disc, std::move(*Index), std::move(*Arctangents)};
	// Platforms and rides; boxes and a platform over layers; Lahan.
	const std::pair<int32, const char*> Digests[] = {
		{317, "bb3b5459256469cc4ed359cb570d77b2801dcf32"},
		{424, "801fe20f286643388e66a7e21238ec13f76f0ac8"},
		{545, "fb9d1cd7d003520763a556a3d178f35ff2d25345"},
		{35, "a618a686c42dd851fbf7c8098991a532d19c69db"},
		{1, "3c3986e9bb436fb439b7a8ca3aa4a656475dccaa"},
	};
	for (const auto& [Number, Expected] : Digests)
	{
		INFO("map " << Number);
		CHECK(RunMap(Files, Number) == Expected);
	}
}
