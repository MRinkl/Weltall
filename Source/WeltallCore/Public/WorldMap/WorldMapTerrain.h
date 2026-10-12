#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	struct FWorldMapTables;

	/** A place in the world: 20.12 units (x east, y down, z south), with the word the original keeps after them. */
	struct FWorldMapVector
	{
		int32 X = 0;
		int32 Y = 0;
		int32 Z = 0;
		int32 W = 0;

		bool operator==(const FWorldMapVector&) const = default;
	};

	/** The terrain types a cell's split line divides (5-7 unused). */
	namespace WorldMapTerrainType
	{
		inline constexpr int32 Land = 0;
		/** Mountains, cliffs and coasts: only the flying Yggdrasil crosses them. */
		inline constexpr int32 Mountain = 1;
		inline constexpr int32 Sea = 2;
		/** Desert sand: every mode crosses it, raising dust. */
		inline constexpr int32 Sand = 3;
		/** Forest: on foot only, and no one mounts or leaves a gear there. */
		inline constexpr int32 Forest = 4;
	}

	/**
	 * The world's terrain: blocks of 2048 units, each 2 x 2 quadrants of 8 x 8 cells of 128 units. A
	 * quadrant holds 9 x 9 vertex words (a cell's height, texture, water and diagonal at its north-west
	 * corner, and its encounter region); a block holds a 16 x 16 grid of cell attributes (a split line
	 * and the terrain type on either side of it).
	 */
	class FWorldMapTerrain
	{
	public:
		static constexpr int32 BlockBytes = 0x710;
		static constexpr int32 BlockSpacing = 0x800;
		static constexpr int32 QuadrantBytes = 0x144;
		static constexpr int32 AttributesOffset = 0x510;
		static constexpr int32 QuadrantSide = 9;
		/** A block's side in whole units. */
		static constexpr int32 BlockSide = 2048;

		/** Reads the terrain file's blocks, one a sector, row by row. */
		WELTALLCORE_API static TResult<FWorldMapTerrain> Parse(std::span<const uint8> File, int32 BlocksX, int32 BlocksZ);

		int32 BlocksX() const { return Width; }
		int32 BlocksZ() const { return Depth; }
		int32 BlockCount() const { return Width * Depth; }

		/** A block's vertex word: quadrant (bit 0 east, bit 1 south), column and row of its 9 x 9. */
		WELTALLCORE_API uint32 VertexWord(int32 Block, int32 Quadrant, int32 Column, int32 Row) const;
		/** A block's cell attribute: cell column and row of its 16 x 16. */
		WELTALLCORE_API uint16 CellAttribute(int32 Block, int32 Column, int32 Row) const;

	private:
		std::vector<uint8> Bytes;
		int32 Width = 0;
		int32 Depth = 0;
	};

	/** The corners of the cell a place lies in: its block, quadrant and north-west vertex. */
	struct FWorldMapCell
	{
		int32 Block = 0;
		int32 Quadrant = 0;
		int32 Column = 0;
		int32 Row = 0;
	};

	/** The ground a scene's terrain makes under the program's rules: where things stand, what they may cross. */
	class FWorldMapGround
	{
	public:
		FWorldMapGround(const FWorldMapTerrain& InTerrain, const FWorldMapTables& InTables)
			: Terrain(&InTerrain), ProgramTables(&InTables)
		{
		}

		const FWorldMapTerrain& Blocks() const { return *Terrain; }

		/** Keeps x and z within the world (one wrap per axis, as the program's). */
		WELTALLCORE_API void Wrap(FWorldMapVector& Point) const;
		/** Folds a 20.12 difference's x and z into half the world either way. */
		WELTALLCORE_API void WrapDelta(FWorldMapVector& Delta) const;
		/** The same for whole units. */
		WELTALLCORE_API void WrapDeltaUnits(int32& X, int32& Z) const;

		/** The cell holding (x, z), found as the program finds it. */
		WELTALLCORE_API FWorldMapCell CellAt(int32 X, int32 Z) const;
		/** A vertex word of the cell holding (x, z): 0, 1 the cell's north corners, 9, 10 its south ones. */
		WELTALLCORE_API uint32 CornerWord(int32 X, int32 Z, int32 Corner) const;

		/** The ground's y (20.12, up is negative) at (x, z) on the cell's triangle. */
		WELTALLCORE_API int32 Height(int32 X, int32 Z) const;
		/** The normal (4.12) of the cell's triangle under (x, z). */
		WELTALLCORE_API std::array<int32, 3> GroundNormal(int32 X, int32 Z) const;
		/** The sea's surface at (x, z): the corners raised by the waves of the phase, in 20.12. */
		WELTALLCORE_API int32 SeaHeight(int32 X, int32 Z, int32 Phase) const;
		/** The cell's attribute word. */
		WELTALLCORE_API uint16 Attribute(int32 X, int32 Z) const;
		/** The terrain type (0-7) on the place's side of its cell's split line. */
		WELTALLCORE_API int32 TerrainType(int32 X, int32 Z) const;
		/** The encounter region (0-15) of the cell. */
		WELTALLCORE_API int32 Region(int32 X, int32 Z) const;
		/** Whether a travel mode may cross a terrain type. */
		WELTALLCORE_API bool CanTraverse(int32 TravelMode, int32 Type) const;

		/** Whether a query found a block outside the world (a place the program never wraps first); the port stops there. */
		bool ReadOutside() const { return bReadOutside; }
		void ClearReadOutside() { bReadOutside = false; }

		const FWorldMapTables& Tables() const { return *ProgramTables; }

	private:
		int32 BlockIndex(int32 X, int32 Z) const;

		const FWorldMapTerrain* Terrain;
		const FWorldMapTables* ProgramTables;
		mutable bool bReadOutside = false;
	};
}
