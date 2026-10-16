#include "Battle/BattleRuntime.h"
#include "Battle/Show/BattleGameHud.h"
#include "Battle/Show/BattlePresentation.h"
#include "Battle/Show/BattleSceneGraphics.h"
#include "Battle/Show/BattleSpriteFiles.h"
#include "Battle/Show/BattleSpriteTextures.h"
#include "Game/GameState.h"
#include "Game/GameStateNames.h"
#include "Game/KernelState.h"
#include "Game/PsxRandom.h"
#include "Graphics/Texture.h"
#include "Graphics/TextureRegion.h"
#include "Input/KernelPad.h"
#include "Support/BattleProgramDisc.h"
#include "Support/BattleSoundRecorder.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <set>
#include <string>

using namespace Weltall;

namespace
{
	/** The combatants' CLUTs lie on row 1C0h + their slot. */
	constexpr int32 SlotClutRows = 0x1C0;
	constexpr int32 SlotCount = 11;
}

TEST_CASE("The party's and the sprite enemies' battle sprites draw cells whose pictures the texture export keys", "[Battle][Content][TextureRegion]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// Fei, Elly and Citan on foot in the woods near Lahan.
	constexpr int32 Map = 15;
	constexpr int32 Formation = 1;
	constexpr std::array<int32, 1> Maps{Map};
	constexpr std::array<uint8, 3> Party{0, 1, 2};
	Testing::FBattleProgramDisc Loaded;
	REQUIRE(Testing::LoadBattleProgramDisc(*Disc, Maps, Loaded));

	FGameState Game = *Loaded.NewGame;
	Game.SetParty({Party[0], Party[1], Party[2]});
	Game.SetJoinedCharacters(0x1FF);
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
	Testing::FBattleSoundRecorder Sound;
	FBattleGameHud Hud(*Loaded.HudResources, Text, FBattleHudSettings{Kernel.WindowColour, Kernel.BattleWindowBlend});
	const auto Presentation = std::make_unique<FBattlePresentation>(Loaded.PresentationFiles(), &Sound, &Hud);
	FBattleRuntimeServices Services;
	Services.Image = &Disc->Image();
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

	// Each combatant's cell, as the drawing keys its quad (its texels, its last ones one before the far edges), and the keys
	// it was drawn with: a flash or a tint may colour it in some frames.
	std::map<FTextureRegion, std::set<uint64>> Drawn;
	for (int32 Frame = 0; Frame < 1500 && !(*Runtime)->IsOver(); ++Frame)
	{
		for (int32 Count = (*Runtime)->VblanksPerFrame(); Count > 0; --Count)
		{
			Pad.Vblank(FPadState{static_cast<uint16>(Frame % 24 < 2 ? 1u << 14 : 0u)}, Game.PlayTime());
		}
		(*Runtime)->Frame();
		REQUIRE((*Runtime)->Fault() == EBattleFault::None);
		if (!Presentation->HasBegun())
		{
			continue;
		}
		const FBattleShowFrame& Shown = Presentation->Show.Frame();
		if (Shown.Vram == nullptr)
		{
			continue;
		}
		for (const FBattleShowQuad& Entry : Shown.Quads)
		{
			const FSpriteQuad& Quad = Entry.Quad;
			const FClutPosition Clut = FClutPosition::FromAttribute(Quad.Clut);
			if (Shown.Drawers[Entry.Drawer].Kind != EBattleShowDrawer::Sprite || Clut.Y < SlotClutRows || Clut.Y >= SlotClutRows + SlotCount)
			{
				continue;
			}
			const auto Texel = [](int32 Value) { return static_cast<uint8>(std::clamp(Value, 0, 255)); };
			const FTextureRegion Cell = TextureRegionBetween(PackTextureView(FTexturePage::FromAttribute(Quad.TexturePage), Clut),
				Texel(std::min(Quad.U0, Quad.U1)), Texel(std::min(Quad.V0, Quad.V1)), Texel(std::max(Quad.U0, Quad.U1) - 1), Texel(std::max(Quad.V0, Quad.V1) - 1));
			Drawn[Cell].insert(TextureRegionKey(*Shown.Vram, Cell));
		}
	}
	REQUIRE(Drawn.size() > 50);

	FBattleSpriteFiles Files = *FBattleSpriteFiles::Load(Disc->Image(), *Loaded.Index);
	std::set<uint64> Exported;
	std::set<uint64> Attacks;
	std::set<uint64> Enemies;
	const FBattleSpriteCellVisit Take = [&](const FVram& Vram, const FTextureRegion& Cell, const std::string& Sheet)
	{
		const uint64 Key = TextureRegionKey(Vram, Cell);
		Exported.insert(Key);
		if (Sheet.starts_with("attacks-"))
		{
			Attacks.insert(Key);
		}
		else if (Sheet.starts_with("sprite-"))
		{
			Enemies.insert(Key);
		}
	};
	for (const int32 Character : Party)
	{
		REQUIRE_FALSE(ForEachBattleCharacterCell(Files, Character, Take));
	}
	int32 SpriteScenes = 0;
	for (const int32 Scene : FBattleSceneGraphics::Scenes(*Loaded.Index))
	{
		const TResult<FBattleSceneGraphics> Graphics = FBattleSceneGraphics::Load(Disc->Image(), *Loaded.Index, Scene);
		REQUIRE(Graphics);
		INFO("scene " << Scene);
		REQUIRE_FALSE(ForEachBattleSceneSpriteCell(*Graphics, Take));
		SpriteScenes += std::any_of(Graphics->Entries().begin(), Graphics->Entries().end(), [](const FBattleSceneEntry& Entry) { return Entry.IsSprite(); }) ? 1 : 0;
	}
	CHECK(SpriteScenes >= 24);

	int32 Missing = 0;
	int32 AttackCells = 0;
	int32 EnemyCells = 0;
	for (const auto& [Cell, Keys] : Drawn)
	{
		AttackCells += std::any_of(Keys.begin(), Keys.end(), [&Attacks](uint64 Key) { return Attacks.contains(Key); }) ? 1 : 0;
		EnemyCells += std::any_of(Keys.begin(), Keys.end(), [&Enemies](uint64 Key) { return Enemies.contains(Key); }) ? 1 : 0;
		const bool bExported = std::any_of(Keys.begin(), Keys.end(), [&Exported](uint64 Key) { return Exported.contains(Key); });
		if (!bExported)
		{
			UNSCOPED_INFO("cell at " << static_cast<int32>(Cell.U) << ", " << static_cast<int32>(Cell.V) << " of view " << Cell.View << ", "
				<< static_cast<int32>(Cell.Width) << " x " << static_cast<int32>(Cell.Height));
		}
		Missing += bExported ? 0 : 1;
	}
	INFO("drawn " << Drawn.size() << ", exported " << Exported.size() << ", attacks' " << AttackCells << ", the enemies' " << EnemyCells);
	CHECK(Missing == 0);
	// The party attacked, from their attack files streamed to the battle's place, and the enemies are sprites.
	CHECK(AttackCells > 0);
	CHECK(EnemyCells > 0);
}
