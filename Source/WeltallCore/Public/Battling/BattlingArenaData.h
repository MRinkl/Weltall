#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/Vram.h"
#include "Media/DiscIdentification.h"
#include "Presentation/DrawList.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	struct FBattlingGearFile;

	/** One of the arena's lightings: day, sunset or night. */
	struct FBattlingLighting
	{
		/** The sky's colour at the top of the screen; it grades down to the far colour. */
		FDrawColor SkyTop;
		/** The light every surface of a gear receives. */
		FDrawColor Back;
		/** What the ground and the wall fade toward with depth. */
		FDrawColor Far;
		/** The ground's and the wall's own colour before the fade. */
		FDrawColor FogBase;
		/** The gears' hit lights are halved. */
		bool bHalvesLights = false;
	};

	/** What the Battling program's own tables hold of the arena's drawing. */
	struct FBattlingArenaTables
	{
		static constexpr int32 Rows = 128;

		/** The arena's round outline: for each row of cells, the first column drawn and the last. */
		std::array<uint8, Rows> FirstColumn{};
		std::array<uint8, Rows> LastColumn{};
		/**
		 * A cell's four corners' texels (u in the low byte, v in the high) from its origin in the page, by bits 16-17 of the
		 * cell: as it lies, mirrored across, mirrored down, or both. In the order: the cell's origin, the next column, the
		 * next row, the far corner.
		 */
		std::array<std::array<uint16, 4>, 4> CellTexels{};
		std::array<FBattlingLighting, 3> Lighting{};
		/**
		 * The HUD's gauge, as points (x, y) the HUD places on player 1's side and turns about the screen for player 2's: its
		 * outline's left end, right end, lower right and lower middle, the bend's top and its lower left, then the second
		 * piece's and the first's top right ends.
		 */
		std::array<std::array<int16, 2>, 8> HudPoints{};
		/** The 16 colours the gears' names are drawn in. */
		std::array<uint16, 16> NamePalette{};
		/**
		 * Where the effects' sprites' frames lie in their pictures, by frame: the texels across of the second and fourth
		 * sets' frames, then of the third's and the fifth's; the rows down of the second and fourth sets' frames, the
		 * third's, then the fifth's, from their picture's first row.
		 */
		std::array<std::array<uint8, 16>, 2> EffectColumns{};
		std::array<std::array<uint8, 16>, 3> EffectRows{};
		/** The fire's colours, by heat. */
		static constexpr std::size_t FireColours = 256;
		std::array<uint16, FireColours> FirePalette{};

		WELTALLCORE_API static TResult<FBattlingArenaTables> Read(const FCdImage& Image, const FGameFileIndex& Index, EDiscRelease Release);
	};

	/**
	 * The arena's pictures (the TIM archive of directory 48): what the program uploads of them as it starts, and where the
	 * ground's four pages, the wall and the gears' shadow lie in VRAM.
	 */
	struct FBattlingArenaTextures
	{
		/** Every upload, in the program's order. */
		std::vector<FVramImage> Uploads;
		/** The ground's pages and their palettes, by bits 18-19 of a cell. */
		std::array<uint16, 4> TerrainPages{};
		std::array<uint16, 4> TerrainCluts{};
		uint16 WallPage = 0;
		uint16 WallClut = 0;
		/** The wall's picture's first row in its page. */
		uint8 WallV = 0;
		uint16 ShadowPage = 0;
		uint16 ShadowClut = 0;
		uint8 ShadowU = 0;
		uint8 ShadowV = 0;
		/** The labels' font's page and palette. */
		uint16 FontPage = 0;
		uint16 FontClut = 0;
		/** The HUD's heat tubes and the heat that fills them: where their pixels lie, and the tubes' palette, which both are drawn with. */
		FVramRect HudTubes;
		FVramRect HudHeat;
		uint16 HudClut = 0;
		/**
		 * The effects' pictures: the first sprite set's twelve frames, each a picture of its own (its
		 * first texel, u in the low byte and v in the high, and its palette) on one page; then the pictures the second and
		 * fourth sets', the third's and the fifth's frames lie in: each one's page (the fourth set subtracts its page's from
		 * the screen), palette and first row in its page.
		 */
		std::array<uint16, 12> FirstSetTexels{};
		std::array<uint16, 12> FirstSetCluts{};
		uint16 FirstSetPage = 0;
		std::array<uint16, 3> SetPages{};
		std::array<uint16, 3> SetCluts{};
		std::array<uint8, 3> SetRows{};
		/**
		 * The trails' and the bands' picture's page, and the row under its palette, where the program puts the gears'
		 * colours as a palette of their own: its first two colours are the gears', its next two hold the
		 * texel each side's trails read in the page, which picks its gear's colour.
		 */
		uint16 TrailPage = 0;
		uint16 TrailClut = 0;
		/** By side: u in the low byte, v in the high. */
		std::array<uint16, 2> TrailTexels{};
		FVramRect TrailPalette;
		/** The tutorial's pointer, 16 x 16: its first texel (u in the low byte, v in the high), palette and page. */
		uint16 PointerTexel = 0;
		uint16 PointerClut = 0;
		uint16 PointerPage = 0;

		WELTALLCORE_API static TResult<FBattlingArenaTextures> Read(const FCdImage& Image, const FGameFileIndex& Index);
	};

	/** One of the effects' five sprite sets, as the effects' load makes it. */
	struct FBattlingSpriteSet
	{
		/** Each frame's first texel in the page, u in the low byte and v in the high, and its palette. */
		std::array<uint16, 16> Texels{};
		std::array<uint16, 16> Cluts{};
		uint16 Page = 0;
		/** The texels across and down a frame covers. */
		uint8 Size = 0;
		/** Half the side of the square about its point a frame is drawn over, in the world's units. */
		int16 Half = 0;
		/** Its texels are drawn as they are; else multiplied by the effect's colour. */
		bool bRaw = true;
	};

	WELTALLCORE_API std::array<FBattlingSpriteSet, 5> MakeBattlingSpriteSets(const FBattlingArenaTables& Tables, const FBattlingArenaTextures& Textures);

	/**
	 * Gears' portraits as the versus screen and the gear choice load them (48/6, read by sector): for each gear its 128-colour
	 * palette, at (200h, 1FFh less the gear), then its 60 x 64 picture (8-bit), the gears' pictures laid out seven to a
	 * column of VRAM from (1A0h, 0).
	 */
	WELTALLCORE_API TResult<std::vector<FVramImage>> ReadBattlingPortraitUploads(const FCdImage& Image, const FGameFileIndex& Index,
		std::span<const uint8> Gears);

	/** Where a gear's portrait's picture lies in VRAM, and its palette's place. */
	WELTALLCORE_API FVramRect BattlingPortraitPlace(int32 Gear);
	WELTALLCORE_API FVramRect BattlingPortraitPalettePlace(int32 Gear);

	/** The fire's palette as the fire's start loads it: its colours blending, the last left out, at (0, 1FDh). */
	WELTALLCORE_API FVramImage BattlingFirePaletteUpload(const FBattlingArenaTables& Tables);

	/** Where the fire's picture lies in VRAM (8-bit, two texels a unit): its rows the screen shows, uploaded as each frame draws it. */
	inline constexpr FVramRect BattlingFirePicturePlace{0x140, 0x100, 0x38, 0x2B};

	/** The fire's picture's rows the screen shows (a palette index a texel, row-major), as a frame uploads them. */
	WELTALLCORE_API FVramImage BattlingFirePictureUpload(std::span<const uint8> Rows);

	/** The palette of the gears' colours the trails and the bands are drawn in, as the program makes it once both gears are loaded. */
	WELTALLCORE_API FVramImage BattlingTrailPaletteUpload(const FBattlingArenaTextures& Textures, const std::array<uint8, 3>& Player1Colour,
		const std::array<uint8, 3>& Player2Colour);

	/** Where the gears' names lie in VRAM (player 1's rows, then player 2's), and their palette. */
	inline constexpr FVramRect BattlingNamesPlace{0x3A0, 0x100, 0x10, 0x10};
	inline constexpr FVramRect BattlingNamePalettePlace{0x3A0, 0x110, 0x10, 1};
	/** Where the gears' icons lie: player 1's 22 x 22 from (380h, 100h), player 2's 16 units to the right; their palettes from row 1F6h. */
	inline constexpr FVramRect BattlingIconsPlace{0x380, 0x100, 0x20, 22};
	inline constexpr int16 BattlingIconPaletteRow = 0x1F6;

	/** The upload of the names' palette, as the HUD's load makes it. */
	WELTALLCORE_API FVramImage BattlingNamePaletteUpload(const FBattlingArenaTables& Tables);

	/** A gear's HUD pictures as loading it on a side uploads them: its icon's palette, its icon (player 1's mirrored left to right) and its name. */
	WELTALLCORE_API std::array<FVramImage, 3> BattlingGearHudUploads(const FBattlingGearFile& File, int32 Side);

	/**
	 * A gear's textures as loading it on a side uploads them: its image list's pictures from the side's page (3C0h for
	 * player 1, 380h for player 2, at VRAM's top) and its palettes from the side's row (1FFh, 1FEh), each at its own
	 * offset from there.
	 */
	WELTALLCORE_API TResult<std::vector<FVramImage>> ReadBattlingGearTextures(const FBattlingGearFile& File, int32 Side);
}
