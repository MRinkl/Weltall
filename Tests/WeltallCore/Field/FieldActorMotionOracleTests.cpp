#include "Field/EventPlacement.h"
#include "Field/FieldActorMotion.h"
#include "Field/FieldMap.h"
#include "Field/FieldSpriteFiles.h"
#include "Field/FieldSprites.h"
#include "Game/PsxRandom.h"
#include "Graphics/GteMath.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"
#include "Support/OracleDigest.h"
#include "Support/SpriteDigest.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;
using Testing::DrawSigned;
using Testing::FOracleDigest;
using Testing::FXorShift32;

// The digest comes from the game's own routines for an actor's animation, its walking velocity, whether it is settled, and the
// ground distance run under the oracle harness on map 1's
// actors, each with its real sprite, over cases drawn from the same generator.

namespace
{
	constexpr int32 MapNumber = 1;
	constexpr uint32 LayoutSeed = 0x1D5C6A31;
	constexpr uint32 CaseSeed = 0xAC70;
	constexpr int32 Cases = 512;
	constexpr uint32 PlacedOrInTheAir = EventActorFlags0::Airborne | EventActorFlags0::Falling | EventActorFlags0::FollowsFloor;
	constexpr uint32 Slopes = 0x420000;

	void SetWhole(int32& Value, int32 Whole)
	{
		Value = static_cast<int32>((static_cast<uint32>(Value) & 0xFFFF) | static_cast<uint32>(Whole) << 16);
	}

	bool TakeFault(FEventContext& Context)
	{
		const bool bFault = Context.Run.Fault != EEventFault::None;
		Context.Run.Fault = EEventFault::None;
		return bFault;
	}
}

TEST_CASE("The actors' motion helpers match the game's", "[Field][Content][Oracle]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Arctangents = FGteArctangents::Read(*Executable);
	REQUIRE(Arctangents);
	auto Map = LoadFieldMap(Disc->Image(), *Index, MapNumber);
	REQUIRE(Map);
	const auto Scripts = FEventScripts::Parse(Map->EventScripts);
	REQUIRE(Scripts);
	const auto SpriteFiles = ParseFieldSpriteFiles(Map->SpriteSets);
	REQUIRE(SpriteFiles);

	FPsxRandom Random(LayoutSeed);
	FEventContext Context;
	Context.Walkmesh = &Map->Walkmesh;
	Context.Random = &Random;
	Context.Actors = CreateFieldActors(Map->Actors);
	const int32 Count = Scripts->ActorCount();
	Context.States.resize(static_cast<std::size_t>(Count));
	for (std::size_t Actor = 0; Actor < Context.States.size(); ++Actor)
	{
		Context.States[Actor].Reset(Random.Next());
		REQUIRE(PlaceActorOnLoad(Context.States[Actor], Context.Actors[Actor], Map->Walkmesh));
	}
	const FPartySpriteFiles NoParty(Disc->Image(), *Index);
	FFieldSprites Sprites(*SpriteFiles, Map->SpriteSlots, NoParty, Context.States, Disc->Image(), *Index);
	Sprites.System().Random = &Random;
	Sprites.System().Arctangents = &*Arctangents;
	Context.Models = &Sprites;
	const int32 FileCount = static_cast<int32>(SpriteFiles->size());
	for (int32 Actor = 0; Actor < Count; ++Actor)
	{
		FFieldSpriteRequest Request;
		Request.Actor = Actor;
		Request.Slot = Actor % FileCount;
		Request.Position = Context.States[static_cast<std::size_t>(Actor)].Position;
		Request.RecordHeight = static_cast<int16>(Context.Actors[static_cast<std::size_t>(Actor)].Translation[1]);
		REQUIRE(Sprites.LoadSprite(Request));
	}

	FXorShift32 Draw(CaseSeed);
	FOracleDigest Digest;
	for (int32 Case = 0; Case < Cases; ++Case)
	{
		const int32 Actor = static_cast<int32>(Draw.Next() % static_cast<uint32>(Count));
		const int32 Other = static_cast<int32>(Draw.Next() % static_cast<uint32>(Count));
		FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
		FSprite& Sprite = *Sprites.ActorSprite(Actor);
		const int32 AnimationCount = (*SpriteFiles)[static_cast<std::size_t>(Actor % FileCount)].AnimationCount((*SpriteFiles)[static_cast<std::size_t>(Actor % FileCount)].Tables());
		Context.Actors[static_cast<std::size_t>(Actor)].Flags = static_cast<uint16>(Draw.Next());
		State.Flags0 = Draw.Next();
		if (Draw.Next() % 4 != 0)
		{
			State.Flags0 &= ~PlacedOrInTheAir;
		}
		State.Flags1 = Draw.Next();
		if (Draw.Next() % 4 != 0)
		{
			State.Flags1 &= ~EventActorFlags1::Gear;
		}
		State.Layer = static_cast<uint16>(Draw.Next() % 5);
		State.FloorAttributes = Draw.Next();
		if (Draw.Next() % 4 != 0)
		{
			State.FloorAttributes &= ~Slopes;
		}
		for (int32& Component : State.Step)
		{
			Component = static_cast<int32>(Draw.Next());
		}
		if (Draw.Next() % 4 != 0)
		{
			State.Step = {};
		}
		State.LinkedActor = static_cast<uint8>(Draw.Next());
		if (Draw.Next() % 4 != 0)
		{
			State.LinkedActor = FEventContext::NoActor;
		}
		State.SpeedDivisor = static_cast<uint16>(1 + Draw.Next() % 0x400);
		if (Draw.Next() % 8 == 0)
		{
			State.SpeedDivisor = 0;
		}
		State.Scale[0] = static_cast<int16>(DrawSigned(Draw, 14));
		State.Scale[2] = static_cast<int16>(DrawSigned(Draw, 14));
		State.Status = Draw.Next();
		const int32 X = DrawSigned(Draw, 16);
		const int32 Z = DrawSigned(Draw, 16);
		const int32 OtherX = DrawSigned(Draw, 16);
		const int32 OtherZ = DrawSigned(Draw, 16);
		SetWhole(State.Position[0], X);
		SetWhole(State.Position[2], Z);
		if (Other != Actor)
		{
			FEventActorState& OtherState = Context.States[static_cast<std::size_t>(Other)];
			SetWhole(OtherState.Position[0], OtherX);
			SetWhole(OtherState.Position[2], OtherZ);
		}
		int16 ActionMode = static_cast<int16>(DrawSigned(Draw, 16));
		if (Draw.Next() % 4 != 0)
		{
			ActionMode = 0;
		}
		Context.Movement.ActionMode = ActionMode;
		Context.Movement.JumpAnimation = static_cast<int16>(Draw.Next() % static_cast<uint32>(AnimationCount));
		Context.Movement.bFirstTickDone = Draw.Next() % 4 != 0;
		Sprite.Speed = DrawSigned(Draw, 20);
		Sprite.SetTimeScale(static_cast<int32>(1 + Draw.Next() % 0xFFF));
		Sprite.Heading = static_cast<int16>(Draw.Next());
		Sprite.Velocity[0] = static_cast<int32>(Draw.Next());
		Sprite.Velocity[2] = static_cast<int32>(Draw.Next());
		const uint32 Choice = Draw.Next() % 4;
		const int32 Any = static_cast<int32>(Draw.Next() % static_cast<uint32>(AnimationCount));
		const int32 Animation = Choice == 0 ? 0xFF : Choice == 1 ? 3 % AnimationCount : Choice == 2 ? Context.Movement.JumpAnimation : Any;
		const int32 Direction = static_cast<int32>(Draw.Next() & 0xFFFF);

		PlayActorAnimation(Context, Actor, Animation);
		Digest.I32(State.Flags0);
		if (TakeFault(Context))
		{
			Digest.U8(1);
		}
		else
		{
			Digest.U8(0);
			Testing::DigestSprite(Digest, Sprite);
		}
		SetActorWalkVelocity(Context, Actor, Direction);
		if (TakeFault(Context))
		{
			Digest.U8(1);
		}
		else
		{
			Digest.U8(0);
			Digest.I32(Sprite.Velocity[0], Sprite.Velocity[2], Sprite.Speed);
			Digest.I16(Sprite.Heading);
		}
		Digest.U8(IsActorSettled(State, Context.Movement) ? 1 : 0);
		Digest.I32(ActorGroundDistance(Context, Actor, Other));
	}
	CHECK(Digest.Hex() == "e437f0e7ed579a3f849b94493ba7d32d6a2af661");
}
