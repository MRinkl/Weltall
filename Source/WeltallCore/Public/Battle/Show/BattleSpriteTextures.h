#pragma once

#include "Foundation/Result.h"
#include "Graphics/TextureRegion.h"

#include <functional>
#include <optional>
#include <string>

namespace Weltall
{
	class FBattleSceneGraphics;
	class FBattleSpriteFiles;
	class FVram;

	/** A cell a frame of a Battle sprite draws, with the VRAM it reads and the sheet of its file and palette set it belongs on. */
	using FBattleSpriteCellVisit = std::function<void(const FVram& Vram, const FTextureRegion& Cell, const std::string& Sheet)>;

	/**
	 * Every frame of character Character's battle sprite file and of its attack file under each of their palette sets, built
	 * as Battle's show builds them: the sprite at the first party member's place on row 1C0h, its attacks streamed to the
	 * battle's place, each through its own palette set as the kernel uploads it on taking the file. Each part's cell goes to
	 * Visit with the sheet "cells-P" or "attacks-cells-P" of palette set P.
	 */
	WELTALLCORE_API std::optional<FError> ForEachBattleCharacterCell(const FBattleSpriteFiles& Files, int32 Character, const FBattleSpriteCellVisit& Visit);

	/**
	 * Every frame of a scene's sprite enemies under the palette set each entry takes, built as Battle's show builds them,
	 * their image sets placed as the setup places them (FBattleSceneImageSets). Each part's cell goes to Visit with the sheet
	 * "sprite-EE-cells-P" of entry EE and palette set P.
	 */
	WELTALLCORE_API std::optional<FError> ForEachBattleSceneSpriteCell(const FBattleSceneGraphics& Scene, const FBattleSpriteCellVisit& Visit);

	/**
	 * Every frame of the common effects' sprite file (directory 44 file 2) under each palette set, built as their spawned
	 * sprites build them: at the common effects' place over the images file 1 uploads, through the colours those leave
	 * at their CLUT. Each part's cell goes to Visit with the sheet "cells-P".
	 */
	WELTALLCORE_API std::optional<FError> ForEachBattleCommonEffectCell(const FBattleSpriteFiles& Files, const FBattleSpriteCellVisit& Visit);

	/**
	 * Every frame of effect Effect's sprite file and of the sprite resources among its sections, under each palette set,
	 * over the images and colours its VRAM stream uploads: a streamed file as its actor plays it (at the battle's place,
	 * through its own palettes, "cells-P"), a resident one and each section's as the effect's spawned sprites draw them
	 * (at the effect's place through the stream's colours, "cells-P" and "section-N-cells-P"). The build tool's stand-ins
	 * give none.
	 */
	WELTALLCORE_API std::optional<FError> ForEachBattleEffectCell(const FBattleSpriteFiles& Files, int32 Effect, const FBattleSpriteCellVisit& Visit);
}
