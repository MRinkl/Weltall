#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;

	/**
	 * One entry of a scene's graphics, one per enemy type. A sprite entry's data is a Battle sprite file
	 * (entries may name the same one) and its images an image set (or, 7 and below, the image set of that
	 * number among those the entries before it loaded, counted from 0); a model entry's data is its motion
	 * pack, kept for the battle, and its images its model pack, read once, or (7 and below) the entry
	 * whose model it is a part of.
	 */
	struct FBattleSceneEntry
	{
		/** An offset in the file, or (7 and below) the entry the data is shared with. */
		uint32 Data = 0;
		uint32 Images = 0;
		/** 0 a sprite, else a 3D model. */
		uint8 Kind = 0;
		uint8 Byte9 = 0;
		uint8 Byte10 = 0;
		uint8 PaletteSet = 0;

		bool IsSprite() const { return Kind == 0; }
		/** Whether the images word is an offset into the file rather than an entry or a set's number. */
		bool HasOwnImages() const { return Images > 7; }
	};

	/**
	 * A scene's graphics file (directory 13, file 2n + 3), as the setup reads it: the entry count, the
	 * VRAM column its image sets start at (x = 140h + 40h each, y 100h), the end of the data the battle
	 * keeps (the entries' data, copied from after the entries), and the entries.
	 */
	class FBattleSceneGraphics
	{
	public:
		static constexpr int32 SceneDirectory = 13;
		static constexpr std::size_t EntriesOffset = 8;
		static constexpr std::size_t EntrySize = 0xC;

		WELTALLCORE_API static TResult<FBattleSceneGraphics> Parse(std::vector<uint8> File);
		WELTALLCORE_API static TResult<FBattleSceneGraphics> Load(const FCdImage& Image, const FGameFileIndex& Index, int32 Scene);
		/** The scenes whose graphics the disc holds, in order (a scene's place in directory 13 may be empty). */
		WELTALLCORE_API static std::vector<int32> Scenes(const FGameFileIndex& Index);

		std::span<const uint8> Bytes() const { return File; }
		uint8 FirstColumn() const { return File[1]; }
		/** Where the data the battle keeps ends; it starts after the entries. */
		uint32 KeptEnd() const { return End; }
		const std::vector<FBattleSceneEntry>& Entries() const { return SceneEntries; }

	private:
		std::vector<uint8> File;
		uint32 End = 0;
		std::vector<FBattleSceneEntry> SceneEntries;
	};

	/** An image set as the kernel reads it: images side by side, each 40h VRAM columns apart. */
	struct FBattleImageSetImage
	{
		uint16 Width = 0;
		uint16 Height = 0;
		/** Where its 16-bit units start in the set's file. */
		uint32 Units = 0;
	};

	/** The images of the set at Offset of File; an error when a count or an image runs past the file. */
	WELTALLCORE_API TResult<std::vector<FBattleImageSetImage>> ReadBattleImageSet(std::span<const uint8> File, uint32 Offset);
}
