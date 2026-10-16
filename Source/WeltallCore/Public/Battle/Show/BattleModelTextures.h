#pragma once

#include "Foundation/Result.h"
#include "Graphics/TexturePictures.h"

#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	class FVram;
	struct FBattleObjectTables;

	/** What the Battle program loads as 3D objects, each with textures of its own. */
	enum class EBattleModelSource : uint8
	{
		/** Stage Number of directory 15. */
		Stage,
		/** The model enemies of scene Number's graphics (directory 13, file 2n + 3). */
		SceneEnemies,
		/** Gear Number of the program's table, with variant Variant (0: none). */
		Gear,
		/** Special model Number of directory 40. */
		Special,
	};

	/**
	 * Loads a source's models into Vram as Battle's objects load them (FBattleObjects::Create: their textures at the place
	 * the program gives that kind of object, a gear's variant after it) and gives their texture regions with their loaded
	 * colours (GearTextureRegions, GearTextureSeeds), so a picture keyed from them has the key a battle's objects teach.
	 * A scene's enemy sprites are not models and give none.
	 */
	WELTALLCORE_API TResult<std::vector<FTexturePictureSeed>> LoadBattleModelTextures(const FCdImage& Image, const FGameFileIndex& Index,
		const FBattleObjectTables& Tables, EBattleModelSource Source, int32 Number, int32 Variant, FVram& Vram);
}
