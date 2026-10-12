#pragma once

#include "Foundation/Types.h"

#include <array>
#include <optional>
#include <vector>

namespace Weltall
{
	class FBattleObjects;
	class FBattleSceneGraphics;

	/**
	 * The setup's walk of a scene's entries: the VRAM column each entry's images take on
	 * row 100h, from the scene's first column (140h + 40h each). A sprite entry's image set takes a column
	 * for each image (the walk goes back to 0 past 2C0h), a model entry with images of its own one; an
	 * entry that names another's images takes none. Nothing where a set's count runs past the file.
	 */
	WELTALLCORE_API std::optional<std::vector<int16>> BattleSceneColumns(const FBattleSceneGraphics& Scene);

	/** The model enemies an entry of the scene made: their slots in the order they were made. */
	struct FBattleSceneModels
	{
		std::array<uint8, 8> Slots{};
		int32 Count = 0;
		/** The entry's model enemies' data runs past the file. */
		bool bOutsideFile = false;
	};

	/**
	 * The setup's model enemies of entry Index, a model with images of its own: the
	 * slots of its type (Types, the slots' occupants) and of the later entries that are parts of it (their
	 * types become this entry's), each made an object of the entry's model and kept sequences, its textures
	 * at Column on row 100h and its CLUT on a row of its own, the first object's model kept for the next
	 * part, the archive shared.
	 */
	WELTALLCORE_API FBattleSceneModels CreateBattleSceneModels(FBattleObjects& Objects, const FBattleSceneGraphics& Scene, std::size_t Index,
		int16 Column, std::array<uint8, 11>& Types);
}
