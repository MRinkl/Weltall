#include "Support/FieldOracleMap.h"

#include "Field/EventMachine.h"
#include "Field/EventPlacement.h"
#include "Field/FieldDiscFiles.h"
#include "Field/FieldMap.h"
#include "Field/FieldPartySprites.h"
#include "Field/FieldSpriteFiles.h"
#include "Field/FieldSprites.h"
#include "Game/PsxRandom.h"
#include "Media/PsxExecutable.h"
#include "Support/EventStateLayout.h"
#include "Support/FieldScrollBytes.h"
#include "Support/FieldSongsModel.h"
#include "Support/FieldStateBytes.h"
#include "Support/NewGameParty.h"
#include "Support/OracleDigest.h"
#include "Support/SpriteDigest.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <utility>

namespace Weltall::Testing
{
	namespace
	{
		constexpr uint32 Seed = 0x1D5C6A31;
		/** A party slot's buffer, among the sprite files uploads and sprites are digested by: this plus the slot. */
		constexpr uint8 PartyBufferFile = 0xF0;
		constexpr uint8 NoFile = 0xFE;
		constexpr int32 GearModeBits = 0xC000;
		/** Where the original's state keeps the addresses of its ride block, its movement bounds and its model parts' values. */
		constexpr std::size_t StateBlocks = 0x110;
		constexpr std::size_t StateBlocksSize = 12;
		/** SpriteDraw bit 7: the ride block is made; Status bit 12: the bounds are. */
		constexpr uint32 RideMade = 0x80;
		constexpr uint32 BoundsMade = 0x1000;

		void DigestScripts(FOracleDigest& Digest, const FEventContext& Context, const FPsxRandom& Random, const FFieldSongsModel& Songs)
		{
			for (uint32 Address = 0; Address < FEventVariables::AddressEnd; Address += 2)
			{
				Digest.I16(Context.Variables.Raw(Address));
			}
			for (std::size_t Actor = 0; Actor < Context.States.size(); ++Actor)
			{
				const FEventActorState& State = Context.States[Actor];
				// The original's heap addresses there digest as zeros; the ride and the bounds by what they hold once made.
				auto Bytes = ToOriginalLayout(State);
				std::fill_n(Bytes.begin() + StateBlocks, StateBlocksSize, uint8{0});
				for (const uint8 Byte : Bytes)
				{
					Digest.U8(Byte);
				}
				if ((State.SpriteDraw & RideMade) != 0)
				{
					Digest.I16(State.Ride.PlatformRotation[0], State.Ride.PlatformRotation[1], State.Ride.PlatformRotation[2], State.Ride.Distance);
				}
				if ((State.Status & BoundsMade) != 0)
				{
					for (const int16 Value : State.MoveBounds)
					{
						Digest.I16(Value);
					}
				}
				// The MIMe weights once the scripts have set any.
				const bool bWeighed = std::any_of(State.PartValues.begin(), State.PartValues.end(), [](int32 Value) { return Value != 0; });
				if ((Context.Actors[Actor].Flags & FieldActorFlag::VertexAnimation) != 0 && bWeighed)
				{
					for (const int32 Value : State.PartValues)
					{
						Digest.I32(Value);
					}
				}
			}
			for (const FFieldActor& Record : Context.Actors)
			{
				for (const std::array<int16, 3>& Row : Record.Matrix)
				{
					Digest.I16(Row[0], Row[1], Row[2]);
				}
				Digest.I32(Record.Translation[0], Record.Translation[1], Record.Translation[2], Record.Position[0], Record.Position[1], Record.Position[2]);
				Digest.I16(Record.Rotation[0], Record.Rotation[1], Record.Rotation[2], Record.Flags, Record.DrawMode);
			}
			Digest.U8(Context.PartyActors[0], Context.PartyActors[1], Context.PartyActors[2], Context.LeaderActor, Context.Camera.FollowedActor);
			for (const uint8 Byte : FieldStateBytes(Context, Songs))
			{
				Digest.U8(Byte);
			}
			Digest.U8(static_cast<uint8>(Context.Gears.Count));
			for (int32 Gear = 0; Gear < Context.Gears.Count; ++Gear)
			{
				Digest.I16(Context.Gears.FileNumbers[static_cast<std::size_t>(Gear)]);
			}
			Digest.I32(Random.Seed());
		}

		void DigestParty(FOracleDigest& Digest, const FEventContext& Context, const FPartySpriteFiles& Party, const FGameState& Game)
		{
			Digest.U8(Context.PartyCharacters[0], Context.PartyCharacters[1], Context.PartyCharacters[2]);
			Digest.I32(Party.File(0), Party.File(1), Party.File(2), static_cast<uint32>(Party.Kind()), Context.MapNumber, Context.PilotSpriteBase);
			Digest.U8(Context.GearActors[0], Context.GearActors[1], Context.GearActors[2], Context.MemberLoad.bPending ? 1 : 0xFF, Context.bExtraAnimationsLoading ? 1 : 0);
			Digest.Raw(Game.Block());
			for (int32 Slot = 0; Slot < FPartySpriteFiles::Slots; ++Slot)
			{
				const std::span<const uint8> Bytes = Party.Buffer(Slot).From(0);
				Digest.Raw(Bytes);
				for (std::size_t Zero = Bytes.size(); Zero < FPartySpriteFiles::BufferSize; ++Zero)
				{
					Digest.U8(0);
				}
			}
		}

		void DigestSprites(FOracleDigest& Digest, const FFieldSprites& Sprites, int32 Actors, std::span<const FSpriteFile> Files, const FPartySpriteFiles& Party, bool bDrawn)
		{
			const auto FileOf = [Files, &Party](const FSpriteFile* File) {
				for (std::size_t Index = 0; Index < Files.size(); ++Index)
				{
					if (File == &Files[Index])
					{
						return static_cast<uint8>(Index);
					}
				}
				for (int32 Slot = 0; Slot < FPartySpriteFiles::Slots; ++Slot)
				{
					if (File == &Party.Buffer(Slot))
					{
						return static_cast<uint8>(PartyBufferFile + Slot);
					}
				}
				return NoFile;
			};
			for (int32 Actor = 0; Actor < Actors; ++Actor)
			{
				const FSprite* Sprite = Sprites.SpriteOf(Actor);
				if (Sprite == nullptr)
				{
					Digest.U8(0);
					continue;
				}
				const FSpriteFile* Alternate = Sprite->AlternateFile;
				Digest.U8(1, FileOf(Sprite->PrimaryFile), Alternate == nullptr ? 0 : Alternate == Sprites.ExtraAnimationsOf(Actor) ? 1 : 2);
				DigestSprite(Digest, *Sprite, bDrawn);
			}
			for (const FKernelTaskEntry& Entry : Sprites.System().Tasks())
			{
				const FSpriteTask* Task = Entry.Sprite.get();
				const bool bOwnerMatches = Task->Owner != nullptr && (Task->Marks & 0x1FFFFFFF) == FSpriteSystem::OwnerNumber(*Task->Owner);
				Digest.U8(1, Task->Marks >> 29 & 3, bOwnerMatches ? 1 : 0);
				DigestSprite(Digest, Task->Sprite, bDrawn);
			}
			Digest.U8(0);
			for (const bool bNow : {false, true})
			{
				for (const FSpriteUpload& Upload : Sprites.System().Uploads())
				{
					if (Upload.bNow != bNow)
					{
						continue;
					}
					if (Upload.File != nullptr)
					{
						Digest.U8(FileOf(Upload.File));
						Digest.I32(Upload.Offset);
					}
					else
					{
						Digest.U8(NoFile);
					}
					Digest.I16(Upload.Rect.X, Upload.Rect.Y, Upload.Rect.Width, Upload.Rect.Height);
				}
				Digest.U8(0xFD);
			}
		}
	}

	bool LoadOracleFieldFiles(const FIso9660& Disc, std::optional<FOracleFieldFiles>& Files)
	{
		const auto Executable = ReadBootExecutable(Disc);
		if (!Executable)
		{
			return false;
		}
		auto Index = FGameFileIndex::Read(*Executable);
		auto Arctangents = FGteArctangents::Read(*Executable);
		if (!Index || !Arctangents)
		{
			return false;
		}
		FGameState NewGame;
		if (!CopyNewGameParty(Disc.Image(), *Index, NewGame))
		{
			return false;
		}
		auto CockpitHud = FFieldCockpitHudLayout::Read(Disc.Image(), *Index, EDiscRelease::UsaDisc1);
		if (!CockpitHud)
		{
			return false;
		}
		Files.emplace(FOracleFieldFiles{&Disc, std::move(*Index), std::move(*Arctangents), NewGame, std::nullopt, {}, std::move(*CockpitHud)});
		Files->EnteredParty.emplace(Disc.Image(), Files->Index);
		return !Files->EnteredParty->LoadForMap(false, Files->NewGame, Files->EnteredFieldParty);
	}

	std::string RunOracleMap(const FOracleFieldFiles& Files, int32 Number, std::optional<std::span<const uint8>> ScriptSection, int32 Ticks, int32 SpriteTicks)
	{
		auto Map = LoadFieldMap(Files.Disc->Image(), Files.Index, Number);
		REQUIRE(Map);
		const auto Scripts = FEventScripts::Parse(ScriptSection ? *ScriptSection : std::span<const uint8>(Map->EventScripts));
		REQUIRE(Scripts);
		const auto SpriteFiles = ParseFieldSpriteFiles(Map->SpriteSets);
		REQUIRE(SpriteFiles);
		FPsxRandom Random(Seed);
		FGameState Game = Files.NewGame;
		// The party's files as the field loads them entering the map: the same for every map on foot.
		FPartySpriteFiles Party = *Files.EnteredParty;
		FEventContext Context;
		Context.PartyCharacters = Files.EnteredFieldParty;
		Context.Scripts = &*Scripts;
		Context.MapNumber = Number;
		Context.RunningMap = Number;
		Context.Zones = Map->Zones;
		Context.MapModels = Map->Models;
		Context.Walkmesh = &Map->Walkmesh;
		Context.Arctangents = &Files.Arctangents;
		FFieldDiscFiles DiscFiles(&Files.Disc->Image(), &Files.Index);
		Context.Files = &DiscFiles;
		Context.Random = &Random;
		Context.GameState = &Game;
		Context.Options = FEventOptions::Original();
		Context.Actors = CreateFieldActors(Map->Actors);
		Context.States.resize(static_cast<std::size_t>(Scripts->ActorCount()));
		for (std::size_t Actor = 0; Actor < Context.States.size(); ++Actor)
		{
			FEventActorState& State = Context.States[Actor];
			State.Reset(Random.Next());
			REQUIRE(PlaceActorOnLoad(State, Context.Actors[Actor], Map->Walkmesh));
		}
		if ((Number & GearModeBits) != 0)
		{
			Party = FPartySpriteFiles(Files.Disc->Image(), Files.Index);
			REQUIRE_FALSE(Party.LoadForMap(true, Game, Context.PartyCharacters));
		}
		Context.PartySprites = &Party;
		const auto Messages = FMessageTable::Parse(Map->Messages);
		REQUIRE(Messages);
		Context.Messages.Table = *Messages;
		Context.Messages.EnterMap();
		FFieldSprites Sprites(*SpriteFiles, Map->SpriteSlots, Party, Context.States, Files.Disc->Image(), Files.Index);
		Sprites.System().Random = &Random;
		Sprites.System().Arctangents = &Files.Arctangents;
		Context.Models = &Sprites;
		FFieldSongsModel Songs;
		Context.Songs = &Songs;
		if (const auto Error = FEventMachine::InitMap(Context))
		{
			FAIL(Error->Message);
		}
		for (int32 Tick = 0; Tick < Ticks; ++Tick)
		{
			if (const auto Error = FEventMachine::Update(Context))
			{
				FAIL(Error->Message);
			}
		}
		for (int32 Tick = 0; Tick < SpriteTicks; ++Tick)
		{
			for (int32 Actor = 0; Actor < Scripts->ActorCount(); ++Actor)
			{
				if (FSprite* Sprite = Sprites.ActorSprite(Actor))
				{
					Sprites.System().TickAnimation(*Sprite);
				}
			}
			Sprites.System().RunTasks();
		}
		Sprites.System().BuildPendingFrames();
		if (const auto Error = Sprites.System().FaultError())
		{
			FAIL(Error->Message);
		}
		FOracleDigest Digest;
		DigestOracleMap(Digest, Context, Random, Songs, Party, Game, Sprites, *SpriteFiles);
		return Digest.Hex();
	}

	void DigestOracleMap(FOracleDigest& Digest, const FEventContext& Context, const FPsxRandom& Random, const FFieldSongsModel& Songs,
		const FPartySpriteFiles& Party, const FGameState& Game, const FFieldSprites& Sprites, std::span<const FSpriteFile> Files, bool bDrawn)
	{
		DigestScripts(Digest, Context, Random, Songs);
		DigestParty(Digest, Context, Party, Game);
		DigestSprites(Digest, Sprites, static_cast<int32>(Context.States.size()), Files, Party, bDrawn);
		// The scrollers the scripts made and the screen wave while it is on.
		if (!Context.Render.LineScrolls.empty())
		{
			Digest.Raw(LineScrollBytes(Context.Render.LineScrolls));
		}
		if (Context.Render.ScreenWave.bOn)
		{
			Digest.Raw(ScreenWaveBytes(Context.Render.ScreenWave));
		}
	}
}
