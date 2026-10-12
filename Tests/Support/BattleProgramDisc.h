#pragma once

#include "Battle/BattleEvent.h"
#include "Battle/BattleProgram.h"
#include "Battle/BattleResult.h"
#include "Battle/BattleTables.h"
#include "Battle/Show/BattleHudSheet.h"
#include "Battle/Show/BattleEventFiles.h"
#include "Battle/Show/BattleObjectFiles.h"
#include "Battle/Show/BattlePresentation.h"
#include "Battle/Show/BattleShowHud.h"
#include "Battle/Show/BattleShowMarkers.h"
#include "Battle/Show/BattleSpriteFiles.h"
#include "Game/GameState.h"
#include "Graphics/GteMath.h"
#include "Media/GameFileIndex.h"
#include "Text/MesResident.h"

#include <map>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	class FIso9660;
}

namespace Weltall::Testing
{
	/** What the Battle program and its presentation read from the disc, loaded once for a test's battles. */
	struct FBattleProgramDisc
	{
		const FIso9660* Disc = nullptr;
		std::optional<FGameFileIndex> Index;
		std::optional<FBattleTables> Tables;
		std::optional<FBattleProgramTables> Program;
		std::optional<FBattleGrowthTable> Growth;
		std::optional<FBattleEventArchive> Events;
		std::optional<FGteArctangents> Arctangents;
		std::optional<FBattleSpriteFiles> SpriteFiles;
		std::optional<FBattleHudSheet> Sheet;
		std::optional<FBattleHudResources> HudResources;
		std::optional<FBattleMarkerModel> MarkerModel;
		std::optional<FBattleObjectDisc> ObjectFiles;
		std::optional<FBattleEventFiles> EventFiles;
		std::optional<FBattleStartSounds> StartSounds;
		FMesResident Resident;
		std::optional<FGameState> NewGame;
		/** The encounter sections of the maps the test fights on, by map. */
		std::map<int32, std::vector<uint8>> Encounters;

		FBattlePresentationFiles PresentationFiles() const;
	};

	/**
	 * A map number from here on names a world map's encounter region rather than a field map: this, plus sixteen times
	 * the scene group, plus the region.
	 */
	inline constexpr int32 WorldMapRegionBase = 9000;
	inline constexpr int32 WorldMapRegionsPerGroup = 16;

	/** Reads the disc's files, with the encounter sections of Maps, into Loaded, which must stay where it is (the sprite files keep pointers to its index). */
	bool LoadBattleProgramDisc(const FIso9660& Iso, std::span<const int32> Maps, FBattleProgramDisc& Loaded);
}
