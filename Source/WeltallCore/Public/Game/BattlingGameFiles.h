#pragma once

#include "Battling/BattlingArenaData.h"
#include "Battling/BattlingData.h"
#include "Foundation/Result.h"
#include "Sound/SoundFiles.h"

#include <memory>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	class FPsxExecutable;

	/**
	 * What Battling reads as it starts, read from the disc once for a session: the program's tables, the arena's heights,
	 * the gears' table, the messages, the program's effects, and the arena's drawing tables and pictures. The gears' own
	 * files are read for each bout.
	 */
	struct FBattlingGameFiles
	{
		FBattlingProgramTables Tables;
		FBattlingTerrain Terrain;
		FBattlingGearTable GearTable;
		FBattlingMessages Messages;
		FSoundEffectFile Effects;
		FBattlingArenaTables ArenaTables;
		FBattlingArenaTextures ArenaTextures;

		WELTALLCORE_API static TResult<std::unique_ptr<FBattlingGameFiles>> Load(const FCdImage& Image, const FGameFileIndex& Index,
			const FPsxExecutable& Executable, EDiscRelease Release);
	};
}
