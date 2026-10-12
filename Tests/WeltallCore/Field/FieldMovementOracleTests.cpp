#include "Field/EventPlacement.h"
#include "Field/FieldActorMove.h"
#include "Field/FieldMap.h"
#include "Field/FieldSpriteFiles.h"
#include "Field/FieldSprites.h"
#include "Game/PsxRandom.h"
#include "Graphics/GteMath.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/EventStateLayout.h"
#include "Support/GameDiscs.h"
#include "Support/OracleDigest.h"
#include "Support/SpriteDigest.h"

#include <catch_amalgamated.hpp>

#include <string>
#include <vector>

using namespace Weltall;
using Testing::DrawSigned;
using Testing::FOracleDigest;
using Testing::FXorShift32;

// The digests come from the original's first pass over the actors in its actors' update (the floor's
// attributes, the floor's forces and the move, or a frozen actor's animation) run under the
// oracle harness on real maps with the actors' real sprites,
// over ticks whose changes to the actors, the party and the map's movement settings are drawn from
// the same generator.

namespace
{
	constexpr uint32 LayoutSeed = 0x1D5C6A31;
	constexpr int32 Ticks = 96;
	/** Pushes, slow ground, slopes, ladders and steep floors, which half the new places are on. */
	constexpr uint32 InterestingFloors = 0x62C100;
	constexpr std::array<uint32, 15> Flags0Bits = {0x1, 0x10, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x4000, 0x10000, 0x40000, 0x400000, 0x1000000, 0x2000000, 0x4000000};
	constexpr std::array<uint32, 13> Flags1Bits = {0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0x200, 0x400, 0x1000, 0x200000, 0x1000000};
	constexpr uint32 RideRecorded = 0x80;
	constexpr uint32 BoundsSet = 0x1000;
	/** The state's links to other blocks, and the previous position the tick copies before this pass. */
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
		const uint32 Animations = static_cast<uint32>((*SpriteFiles)[static_cast<std::size_t>(Slot)].AnimationCount((*SpriteFiles)[static_cast<std::size_t>(Slot)].Tables()));
		for (int32 Actor = 0; Actor < Count; ++Actor)
		{
			GiveSprite(Context, Sprites, Actor, Slot);
		}

		std::vector<std::vector<uint32>> Interesting(Walkmesh.Layers.size());
		for (std::size_t Layer = 0; Layer < Walkmesh.Layers.size(); ++Layer)
		{
			for (uint32 Triangle = 0; Triangle < Walkmesh.Layers[Layer].Triangles.size(); ++Triangle)
			{
				if ((Walkmesh.Attributes[Walkmesh.Layers[Layer].Triangles[Triangle].Attribute] & InterestingFloors) != 0)
				{
					Interesting[Layer].push_back(Triangle);
				}
			}
		}
		FXorShift32 Draw(0x40BE0000u + static_cast<uint32>(Number));
		const auto DrawRide = [&Draw](FEventActorState& State) {
			for (int16& Angle : State.Ride.PlatformRotation)
			{
				Angle = static_cast<int16>(DrawSigned(Draw, 12));
			}
			State.Ride.Distance = static_cast<int16>(DrawSigned(Draw, 12));
		};
		for (FEventActorState& State : Context.States)
		{
			DrawRide(State);
			State.SpriteDraw |= RideRecorded;
		}
		FOracleDigest Digest;
		for (int32 Tick = 0; Tick < Ticks; ++Tick)
		{
			Context.LeaderActor = static_cast<uint8>(Draw.Next() % static_cast<uint32>(Count));
			std::array<uint32, 2> Party{Draw.Next() % static_cast<uint32>(Count), 0};
			Party[1] = Draw.Next() % static_cast<uint32>(Count);
			for (std::size_t Index = 0; Index < Party.size(); ++Index)
			{
				Context.PartyActors[Index + 1] = Draw.Next() % 3 != 0 ? static_cast<uint8>(Party[Index]) : FEventContext::NoActor;
			}
			Context.PadHeld = static_cast<uint16>(Draw.Next());
			Context.bPlayerControlled = Draw.Next() % 2 != 0;
			Context.Movement.ActionMode = Draw.Next() % 8 == 0 ? static_cast<int16>(DrawSigned(Draw, 16)) : int16{0};
			Context.Movement.JumpAnimation = static_cast<int16>(Draw.Next() % Animations);
			Context.Movement.FloorPushAngle = static_cast<uint16>(Draw.Next());
			Context.Movement.FloorPushScale = static_cast<int16>(DrawSigned(Draw, 14));
			Context.Movement.bFirstTickDone = Draw.Next() % 4 != 0;
			Context.bPartyWalking = Draw.Next() % 8 == 0;
			for (int32 Actor = 0; Actor < Count; ++Actor)
			{
				FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
				FFieldActor& Record = Context.Actors[static_cast<std::size_t>(Actor)];
				FSprite& Sprite = *Sprites.ActorSprite(Actor);
				switch (Draw.Next() % 12)
				{
				case 0:
				{
					const uint32 LayerIndex = Draw.Next() % Walkmesh.Layers.size();
					const FWalkmeshLayer& Layer = Walkmesh.Layers[LayerIndex];
					const std::vector<uint32>& Special = Interesting[LayerIndex];
					const bool bSpecial = Draw.Next() % 2 != 0 && !Special.empty();
					const uint32 Pick = Draw.Next() % static_cast<uint32>(bSpecial ? Special.size() : Layer.Triangles.size());
					const uint32 TriangleIndex = bSpecial ? Special[Pick] : Pick;
					const FGteVector& Corner = Layer.Vertices[Layer.Triangles[TriangleIndex].Vertices[Draw.Next() % 3]];
					const int32 X = Corner[0] + DrawSigned(Draw, 5);
					State.Position[0] = Fixed(X, Draw.Next());
					const int32 Z = Corner[2] + DrawSigned(Draw, 5);
					State.Position[2] = Fixed(Z, Draw.Next());
					const int32 Y = Corner[1] + DrawSigned(Draw, 6);
					State.Position[1] = Fixed(Y, Draw.Next());
					for (int32& Component : State.FloorNormal)
					{
						Component = DrawSigned(Draw, 13);
					}
					State.FloorTriangles[LayerIndex] = static_cast<int16>(TriangleIndex);
					State.Layer = static_cast<uint16>(LayerIndex);
					break;
				}
				case 1:
					for (int32& Component : State.Velocity)
					{
						Component = DrawSigned(Draw, 20);
					}
					break;
				case 2:
					State.MoveDirection = static_cast<uint16>(Draw.Next());
					State.Facing = static_cast<uint16>(Draw.Next());
					break;
				case 3:
					State.Flags0 ^= Flags0Bits[Draw.Next() % Flags0Bits.size()];
					break;
				case 4:
					State.Flags1 ^= Flags1Bits[Draw.Next() % Flags1Bits.size()];
					break;
				case 5:
				{
					State.IdleAnimation = static_cast<uint16>(Draw.Next() % Animations);
					State.Animation = static_cast<uint16>(Draw.Next() % Animations);
					uint16 Request = static_cast<uint16>(Draw.Next() % Animations);
					if (Draw.Next() % 2 != 0)
					{
						Request = 0xFF;
					}
					State.AnimationRequest = Request;
					State.SpeedDivisor = static_cast<uint16>(1 + Draw.Next() % 0x400);
					break;
				}
				case 6:
				{
					uint8 Platform = static_cast<uint8>(Draw.Next() % static_cast<uint32>(Count));
					if (Draw.Next() % 2 != 0)
					{
						Platform = FEventContext::NoActor;
					}
					State.LinkedActor = Platform;
					DrawRide(State);
					State.WordF0 = DrawSigned(Draw, 20);
					break;
				}
				case 7:
				{
					State.Status ^= BoundsSet;
					const int32 X = WholeOf(State.Position[0]);
					const int32 Z = WholeOf(State.Position[2]);
					for (std::size_t Corner = 0; Corner < 4; ++Corner)
					{
						State.MoveBounds[2 * Corner] = static_cast<int16>(X + DrawSigned(Draw, 9));
						State.MoveBounds[2 * Corner + 1] = static_cast<int16>(Z + DrawSigned(Draw, 9));
					}
					break;
				}
				case 8:
				{
					Sprite.Gravity = DrawSigned(Draw, 18);
					Sprite.Weight = static_cast<int16>(0x1000 + Draw.Next() % 0x2000);
					int16 Floor = static_cast<int16>(DrawSigned(Draw, 12));
					if (Draw.Next() % 2 != 0)
					{
						Floor = WholeOf(Sprite.Position[1]);
					}
					Sprite.FloorY = Floor;
					break;
				}
				case 9:
				{
					const uint16 Kind = Draw.Next() % 4 != 0 ? FieldActorFlag::WalkingKind : uint16{0x100};
					uint16 Flags = static_cast<uint16>((Record.Flags & ~0xFC0u) | Kind);
					if (Draw.Next() % 4 != 0)
					{
						Flags |= FieldActorFlag::NoModel;
					}
					Record.Flags = Flags;
					Record.Rotation[1] = static_cast<int16>(Draw.Next());
					break;
				}
				case 10:
					State.TargetHeight = static_cast<int16>(DrawSigned(Draw, 12));
					State.PushCount = static_cast<uint8>(Draw.Next());
					break;
				default:
					break;
				}
			}
			for (int32 Actor = 0; Actor < Count; ++Actor)
			{
				if (const auto Error = MoveFieldActor(Context, Actor))
				{
					FAIL(Error->Message);
				}
			}
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
				for (const int16 Value : State.MoveBounds)
				{
					Digest.I16(Value);
				}
				Digest.I16(Context.Actors[static_cast<std::size_t>(Actor)].Flags);
				Testing::DigestSprite(Digest, *Sprites.SpriteOf(Actor));
			}
		}
		return Digest.Hex();
	}
}

TEST_CASE("Walking actors move as the game's first pass over the actors moves them", "[Field][Content][Oracle]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	auto Arctangents = FGteArctangents::Read(*Executable);
	REQUIRE(Arctangents);
	const FFieldFiles Files{Disc, std::move(*Index), std::move(*Arctangents)};
	// Pushes, slow ground, steep floors and ceilings; ladders and slopes; ladders and pushes over four
	// layers; a clockwise triangle; slopes over three layers.
	const std::pair<int32, const char*> Digests[] = {
		{260, "ca6fa7ff8f3a663433318de583802ee207d5376d"},
		{35, "3e3279de43187c2ebfa5d8397d2e631d70cca794"},
		{551, "8fb2d2233db53b2924e343a56895752ca8c2510b"},
		{326, "f31a75c96bb14e861d4e3eaf260b71a76f496eca"},
		{95, "6e9b8a250688f1d5e1bc4dba25a06121a1e8682b"},
	};
	for (const auto& [Number, Expected] : Digests)
	{
		INFO("map " << Number);
		CHECK(RunMap(Files, Number) == Expected);
	}
}
