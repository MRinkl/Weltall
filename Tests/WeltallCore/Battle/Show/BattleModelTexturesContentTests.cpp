#include "Battle/BattleRuntime.h"
#include "Battle/Show/BattleGameHud.h"
#include "Battle/Show/BattleModelTextures.h"
#include "Battle/Show/BattleObjectsFrame.h"
#include "Battle/Show/BattlePresentation.h"
#include "Battle/Show/BattleSceneGraphics.h"
#include "Battle/Show/BattleStageBackdrop.h"
#include "Battle/Show/BattleStageFiles.h"
#include "Game/GameState.h"
#include "Game/GameStateNames.h"
#include "Game/KernelState.h"
#include "Game/PsxRandom.h"
#include "Graphics/TextureRegion.h"
#include "Input/KernelPad.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/BattleProgramDisc.h"
#include "Support/BattleSoundRecorder.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <map>
#include <set>

using namespace Weltall;

namespace
{
	/** The keys of every picture the texture export takes from Battle's 3D models. */
	std::set<uint64> ExportedBattleModelKeys(const FCdImage& Image, const FGameFileIndex& Index, const FBattleObjectTables& Tables)
	{
		std::set<uint64> Keys;
		const auto Take = [&](EBattleModelSource Source, int32 Number, int32 Variant)
		{
			FVram Vram;
			const auto Regions = LoadBattleModelTextures(Image, Index, Tables, Source, Number, Variant, Vram);
			INFO("source " << static_cast<int32>(Source) << " number " << Number << " variant " << Variant);
			REQUIRE(Regions);
			for (const FTexturePictureSeed& Seed : *Regions)
			{
				Keys.insert(Seed.Palette ? TextureRegionKeyWith(ReadTextureRegionTexels(Vram, Seed.Region), *Seed.Palette) : TextureRegionKey(Vram, Seed.Region));
			}
		};
		for (int32 Stage = 0; Stage < FBattleStageFiles::Count(Index); ++Stage)
		{
			Take(EBattleModelSource::Stage, Stage, 0);
		}
		for (const int32 Scene : FBattleSceneGraphics::Scenes(Index))
		{
			Take(EBattleModelSource::SceneEnemies, Scene, 0);
		}
		for (int32 Gear = 0; Gear < FBattleObjectTables::GearCount; ++Gear)
		{
			for (int32 Variant = 0; Variant <= Tables.Gears[static_cast<std::size_t>(Gear)].LastVariant; ++Variant)
			{
				Take(EBattleModelSource::Gear, Gear, Variant);
			}
		}
		return Keys;
	}

	/** Plays formation Formation of Map, Fei alone in Weltall or Fei, Elly and Citan on foot, Cross now and then, each frame shown to Visit. */
	void PlayBattle(const FIso9660& Disc, int32 Map, int32 Formation, bool bInGear, const std::function<void(const FBattleShowFrame&)>& Visit,
		std::set<uint64>& Exported)
	{
		const std::array<int32, 1> Maps{Map};
		Testing::FBattleProgramDisc Loaded;
		REQUIRE(Testing::LoadBattleProgramDisc(Disc, Maps, Loaded));

		FGameState Game = *Loaded.NewGame;
		Game.SetJoinedCharacters(0x1FF);
		if (bInGear)
		{
			Game.SetParty({0, 0xFF, 0xFF});
			REQUIRE(Game.SetCharacterGear(0, 0));
			Game.SetPartySlotInGear(0, 1);
		}
		else
		{
			Game.SetParty({0, 1, 2});
		}
		FPsxRandom Random(0x1234);
		FKernelPad Pad;
		FGameStateNames Names(Game);
		FMesContext Text;
		Text.Font = &Loaded.Resident.Font;
		Text.Tables = &Loaded.Resident.NameTables;
		Text.State = &Names;
		Text.ButtonMap = MesKernelButtonMap;
		FKernelState Kernel;
		Kernel.InitNewGame();

		FBattleRequest Request;
		const std::vector<uint8>& Encounters = Loaded.Encounters.at(Map);
		std::copy_n(Encounters.begin() + Formation * static_cast<std::ptrdiff_t>(FBattleFormation::RecordSize), FBattleFormation::RecordSize,
			Request.Formation.begin());
		Request.Kind = bInGear ? 5 : 0;
		Testing::FBattleSoundRecorder Sound;
		FBattleGameHud Hud(*Loaded.HudResources, Text, FBattleHudSettings{Kernel.WindowColour, Kernel.BattleWindowBlend});
		const auto Presentation = std::make_unique<FBattlePresentation>(Loaded.PresentationFiles(), &Sound, &Hud);
		FBattleRuntimeServices Services;
		Services.Image = &Disc.Image();
		Services.Index = &*Loaded.Index;
		Services.Game = &Game;
		Services.Random = &Random;
		Services.Pad = &Pad;
		Services.Show = Presentation.get();
		Services.Tables = &*Loaded.Tables;
		Services.Program = &*Loaded.Program;
		Services.Growth = &*Loaded.Growth;
		Services.Events = &*Loaded.Events;
		Services.Arctangents = &*Loaded.Arctangents;
		auto Runtime = FBattleRuntime::Begin(Request, Services);
		REQUIRE(Runtime);
		for (int32 Frame = 0; Frame < 600 && !(*Runtime)->IsOver(); ++Frame)
		{
			for (int32 Count = (*Runtime)->VblanksPerFrame(); Count > 0; --Count)
			{
				Pad.Vblank(FPadState{static_cast<uint16>(Frame % 24 < 2 ? 1u << 14 : 0u)}, Game.PlayTime());
			}
			(*Runtime)->Frame();
			REQUIRE((*Runtime)->Fault() == EBattleFault::None);
			if (Presentation->HasBegun() && Presentation->Show.Frame().Objects != nullptr && Presentation->Show.Frame().Vram != nullptr)
			{
				Visit(Presentation->Show.Frame());
			}
		}
		Exported = ExportedBattleModelKeys(Disc.Image(), *Loaded.Index, Loaded.ObjectFiles->Tables());
	}

	/** A model's picture's key as its load left it, else as the frame draws it. */
	uint64 SeedKey(const FVram& Vram, const FTexturePictureSeed& Seed)
	{
		if (Seed.Key != 0)
		{
			return Seed.Key;
		}
		return Seed.Palette ? TextureRegionKeyWith(ReadTextureRegionTexels(Vram, Seed.Region), *Seed.Palette) : TextureRegionKey(Vram, Seed.Region);
	}
}

TEST_CASE("A battle's 3D models draw from regions whose pictures the texture export keys", "[Battle][Content][TextureRegion]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// Weltall's first battle in Lahan (map 2, formation 2), Fei riding it. Its stage's CLUT animation blends part of its sky
	// strip's texels from before the first frame shown: the strip is named by its picture as the stage's load left it.
	std::map<FTextureRegion, uint64> Drawn;
	std::size_t MostRegions = 0;
	std::size_t Loaded = 0;
	std::set<uint64> Exported;
	PlayBattle(*Disc, 2, 2, true, [&](const FBattleShowFrame& Shown)
	{
		MostRegions = std::max(MostRegions, Shown.Objects->TextureSeeds.size());
		for (const FTexturePictureSeed& Seed : Shown.Objects->TextureSeeds)
		{
			if (!Drawn.contains(Seed.Region))
			{
				Drawn[Seed.Region] = SeedKey(*Shown.Vram, Seed);
				Loaded += Seed.Key != 0 ? 1 : 0;
			}
		}
	}, Exported);
	REQUIRE(MostRegions > 0);
	CHECK(Loaded * 10 > Drawn.size() * 9);
	int32 Missing = 0;
	for (const auto& [Region, Key] : Drawn)
	{
		Missing += Exported.contains(Key) ? 0 : 1;
	}
	INFO("drawn " << Drawn.size() << ", with their loaded keys " << Loaded << ", exported " << Exported.size() << ", not exported " << Missing);
	CHECK(Missing == 0);
}

TEST_CASE("Directory 13 holds the scenes' graphics as files 2n + 3 up to scene 75's", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const std::vector<int32> Scenes = FBattleSceneGraphics::Scenes(*Index);
	CHECK(Scenes.size() == 76);
	CHECK(Scenes.back() == 75);
}

TEST_CASE("A battle stage's panorama and floor dome draw from regions whose pictures the stage's export keys", "[Battle][Content][TextureRegion]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// Map 1's formation 3 stands on stage 3, which has a panorama and a dome.
	std::set<uint64> Keys;
	int32 Textured = 0;
	int32 Covered = 0;
	int32 DomeCells = 0;
	std::set<uint64> Exported;
	std::vector<FFieldBackdropQuad> Strip;
	PlayBattle(*Disc, 1, 3, false, [&](const FBattleShowFrame& Shown)
	{
		const std::vector<FTexturePictureSeed>& Seeds = Shown.Objects->TextureSeeds;
		const auto IsCovered = [&Seeds](uint32 View, int32 FirstU, int32 FirstV, int32 LastU, int32 LastV)
		{
			return std::any_of(Seeds.begin(), Seeds.end(), [&](const FTexturePictureSeed& Seed)
			{
				return Seed.Region.View == View && Seed.Region.Contains(FirstU, FirstV) && Seed.Region.Contains(LastU, LastV);
			});
		};
		// The strip's quads: their last column is a texel, their last row the far edge.
		for (const FFieldBackdrop& Panorama : Shown.Backdrop.Panoramas)
		{
			DrawFieldBackdrop(Panorama, 0, FieldBackdropWidth, Strip);
			for (const FFieldBackdropQuad& Quad : Strip)
			{
				if (Quad.Kind != FFieldBackdropQuad::EKind::Textured)
				{
					continue;
				}
				const uint32 View = PackTextureView(FTexturePage::FromAttribute(Quad.TexturePage), FClutPosition::FromAttribute(Quad.Clut));
				++Textured;
				Covered += IsCovered(View, std::min(Quad.Texels[0][0], Quad.Texels[1][0]), Quad.Texels[0][1], std::max(Quad.Texels[0][0], Quad.Texels[1][0]),
					Quad.Texels[2][1] - 1) ? 1 : 0;
			}
		}
		// The dome's cells read their tile through the texture window: the whole tile is one region.
		for (const FBattleDomePacket& Packet : Shown.Backdrop.Dome)
		{
			if (Packet.bDrawMode || Packet.Quad.Kind != FFieldBackdropQuad::EKind::Textured)
			{
				continue;
			}
			const uint32 View = PackTextureView(FTexturePage::FromAttribute(Packet.Quad.TexturePage), FClutPosition::FromAttribute(Packet.Quad.Clut));
			++DomeCells;
			++Textured;
			Covered += std::any_of(Seeds.begin(), Seeds.end(), [View](const FTexturePictureSeed& Seed) { return Seed.Region.View == View && Seed.Region.Width == 128; })
				? 1 : 0;
		}
		for (const FTexturePictureSeed& Seed : Seeds)
		{
			Keys.insert(SeedKey(*Shown.Vram, Seed));
		}
	}, Exported);
	CHECK(DomeCells > 0);
	CHECK(Textured > DomeCells);
	CHECK(Covered == Textured);
	std::vector<uint64> Missing;
	std::set_difference(Keys.begin(), Keys.end(), Exported.begin(), Exported.end(), std::back_inserter(Missing));
	INFO("drawn " << Keys.size() << ", exported " << Exported.size() << ", not exported " << Missing.size());
	CHECK(Missing.empty());
}

TEST_CASE("A gear variant's pictures take the colours its making leaves them", "[Battle][Content][TextureRegion]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const TResult<FBattleObjectTables> Tables = FBattleObjectTables::Load(Disc->Image(), *Index);
	REQUIRE(Tables);
	// Every seed's colours are those its making left at its CLUT: a colour variant's (gears 4 and 12 bring a CLUT) its own,
	// and a gear's whose parts' textures go over its own (gear 5's variants) theirs.
	int32 Coloured = 0;
	for (int32 Gear = 0; Gear < FBattleObjectTables::GearCount; ++Gear)
	{
		for (int32 Variant = 0; Variant <= Tables->Gears[static_cast<std::size_t>(Gear)].LastVariant; ++Variant)
		{
			INFO("gear " << Gear << " variant " << Variant);
			FVram Vram;
			const auto Seeds = LoadBattleModelTextures(Disc->Image(), *Index, *Tables, EBattleModelSource::Gear, Gear, Variant, Vram);
			REQUIRE(Seeds);
			for (const FTexturePictureSeed& Seed : *Seeds)
			{
				if (!Seed.Palette)
				{
					continue;
				}
				std::array<uint16, 256> Colors{};
				const std::span<const uint16> Loaded = ReadTextureViewClut(Vram, Seed.Region.View, Colors);
				CHECK(std::equal(Loaded.begin(), Loaded.end(), Seed.Palette->begin(), Seed.Palette->end()));
				Coloured += Variant != 0 && (Gear == 4 || Gear == 12) ? 1 : 0;
			}
		}
	}
	CHECK(Coloured > 0);
}
