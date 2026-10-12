#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;

	/**
	 * A part record as the kernel builds it from a sprite sheet's frame for the battle's numbers: its
	 * corner on the screen (the frame's offset plus the place asked for), its texels and size, and the
	 * page and CLUT words it samples.
	 */
	struct FBattleSheetPart
	{
		int16 X = 0;
		int16 Y = 0;
		uint8 U = 0;
		uint8 V = 0;
		uint8 Width = 0;
		uint8 Height = 0;
		uint16 TexturePage = 0;
		uint16 Clut = 0;
	};

	/**
	 * The battle's HUD sheet (the battle's data file, section 1): a sprite sheet of frames, each a list of
	 * textured rectangles, which the HUD, the damage numbers and the
	 * running total draw.
	 */
	class FBattleHudSheet
	{
	public:
		/** The battle's data file (directory 12 file 3), whose first section holds the sheet. */
		static constexpr int32 DataDirectory = 12;
		static constexpr int32 DataFile = 3;

		WELTALLCORE_API static TResult<FBattleHudSheet> Parse(std::vector<uint8> Sheet);
		WELTALLCORE_API static TResult<FBattleHudSheet> Load(const FCdImage& Image, const FGameFileIndex& Index);

		int32 FrameCount() const { return Frames; }
		/**
		 * A sheet's part records: frame Frame's parts at (X, Y), appended to Parts; how many there
		 * were, or nothing for a frame past the sheet's.
		 */
		WELTALLCORE_API std::optional<int32> BuildParts(int32 Frame, int32 X, int32 Y, std::vector<FBattleSheetPart>& Parts) const;

	private:
		std::vector<uint8> Bytes;
		int32 Frames = 0;
	};
}
