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
#include "Foundation/Result.h"

#include <memory>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	class FPsxExecutable;
	struct FGteArctangents;

	/**
	 * What the Battle program reads for every battle, read from the disc once for a session: the rules' tables (the
	 * commands and items, the program's own tables, the growth table, the battle events' scripts) and the presentation's
	 * (the sprite files, the HUD's sheet and resources, the target markers' model, the 3D objects' files, the start sounds).
	 */
	struct FBattleGameFiles
	{
		FBattleTables Tables;
		FBattleProgramTables Program;
		FBattleGrowthTable Growth;
		FBattleEventArchive Events;
		FBattleSpriteFiles SpriteFiles;
		FBattleHudSheet Sheet;
		FBattleHudResources HudResources;
		FBattleMarkerModel MarkerModel;
		FBattleObjectDisc ObjectFiles;
		FBattleEventFiles EventFiles;
		FBattleStartSounds StartSounds;

		/** From the disc and its executable; the files keep pointers to Image and Index, which must outlive them. */
		WELTALLCORE_API static TResult<std::unique_ptr<FBattleGameFiles>> Load(const FCdImage& Image, const FGameFileIndex& Index,
			const FPsxExecutable& Executable);

		/** What the presentation reads, with the disc's image, its index and the kernel's arctangents. */
		WELTALLCORE_API FBattlePresentationFiles PresentationFiles(const FCdImage& Image, const FGameFileIndex& Index,
			const FGteArctangents& Arctangents) const;
	};
}
