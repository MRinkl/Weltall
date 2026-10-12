#include "WorldMap/WorldMapTerrain.h"

#include "Foundation/LittleEndian.h"
#include "Graphics/GteVectors.h"
#include "WorldMap/WorldMapTables.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** A block is 2048 units: 2^23 in 20.12. */
		constexpr int32 BlockShift = 23;
		constexpr int32 BlockUnits = 1 << BlockShift;
		/** The corners' vertex words after the north-west one: north-east, south-west, south-east. */
		constexpr int32 EastCorner = 1;
		constexpr int32 SouthCorner = FWorldMapTerrain::QuadrantSide;
		constexpr int32 SouthEastCorner = FWorldMapTerrain::QuadrantSide + 1;
		/** The vertex word's diagonal bit: set, the cell is split north-west to south-east. */
		constexpr uint32 DiagonalBit = 0x8000;
		/** A cell is 1 << 16 of the height's 1/512 units across; its plane's corner offset there. */
		constexpr int32 CellWidth = 0x10000;
		constexpr int32 EdgeX = 16;
		constexpr int32 SeaEdge = 0x80;
		constexpr int32 SeaPhase = 0x400;
		constexpr int32 WaveStep = 0x200;

		/** The compiler's signed division by 8 of a 32-bit value. */
		int32 DivideBy8(int32 Value)
		{
			return (Value < 0 ? Value + 7 : Value) >> 3;
		}

		int32 Multiply(int32 A, int32 B)
		{
			return static_cast<int32>(static_cast<uint32>(A) * static_cast<uint32>(B));
		}

		int32 HeightOf(uint32 Word)
		{
			return static_cast<int8>(Word & 0xFF);
		}

		/** The height of (x, z) on the plane through Point with the normal, the division truncating as the R3000's. */
		int32 PlaneY(int32 X, int32 Z, int32 PointX, int32 PointY, int32 PointZ, const std::array<int32, 3>& Normal)
		{
			const int32 Numerator = Multiply(-Normal[2], Z - PointZ) - Multiply(Normal[0], X - PointX);
			return Numerator / Normal[1] + PointY;
		}
	}

	TResult<FWorldMapTerrain> FWorldMapTerrain::Parse(std::span<const uint8> File, int32 BlocksX, int32 BlocksZ)
	{
		if (BlocksX <= 0 || BlocksZ <= 0)
		{
			return MakeError("a world map's terrain needs at least one block");
		}
		const std::size_t Count = static_cast<std::size_t>(BlocksX) * static_cast<std::size_t>(BlocksZ);
		if (File.size() < (Count - 1) * BlockSpacing + BlockBytes)
		{
			return MakeError("the world map's terrain file is too short for its blocks");
		}
		FWorldMapTerrain Terrain;
		Terrain.Width = BlocksX;
		Terrain.Depth = BlocksZ;
		Terrain.Bytes.resize(Count * BlockBytes);
		for (std::size_t Block = 0; Block < Count; ++Block)
		{
			std::copy_n(File.begin() + static_cast<std::ptrdiff_t>(Block * BlockSpacing), BlockBytes,
				Terrain.Bytes.begin() + static_cast<std::ptrdiff_t>(Block * BlockBytes));
		}
		return Terrain;
	}

	uint32 FWorldMapTerrain::VertexWord(int32 Block, int32 Quadrant, int32 Column, int32 Row) const
	{
		const std::size_t At = static_cast<std::size_t>(Block) * BlockBytes + static_cast<std::size_t>(Quadrant) * QuadrantBytes
			+ static_cast<std::size_t>(Row * QuadrantSide + Column) * 4;
		return LittleEndian::ReadU32(Bytes, At);
	}

	uint16 FWorldMapTerrain::CellAttribute(int32 Block, int32 Column, int32 Row) const
	{
		const std::size_t At = static_cast<std::size_t>(Block) * BlockBytes + AttributesOffset + static_cast<std::size_t>(Row * 16 + Column) * 2;
		return LittleEndian::ReadU16(Bytes, At);
	}

	void FWorldMapGround::Wrap(FWorldMapVector& Point) const
	{
		const int32 SizeX = Terrain->BlocksX() * BlockUnits;
		const int32 SizeZ = Terrain->BlocksZ() * BlockUnits;
		if (Point.X >= SizeX)
		{
			Point.X -= SizeX;
		}
		if (Point.X < 0)
		{
			Point.X += SizeX;
		}
		if (Point.Z >= SizeZ)
		{
			Point.Z -= SizeZ;
		}
		if (Point.Z < 0)
		{
			Point.Z += SizeZ;
		}
	}

	void FWorldMapGround::WrapDelta(FWorldMapVector& Delta) const
	{
		if (Delta.X < -0x4000000)
		{
			Delta.X += Terrain->BlocksX() * BlockUnits;
		}
		else if (Delta.X > 0x4000000)
		{
			Delta.X -= Terrain->BlocksX() * BlockUnits;
		}
		if (Delta.Z < -0x4000000)
		{
			Delta.Z += Terrain->BlocksZ() * BlockUnits;
		}
		else if (Delta.Z > 0x4000000)
		{
			Delta.Z -= Terrain->BlocksZ() * BlockUnits;
		}
	}

	void FWorldMapGround::WrapDeltaUnits(int32& X, int32& Z) const
	{
		const int32 Block = BlockUnits >> 12;
		if (X < -0x4000)
		{
			X += Terrain->BlocksX() * Block;
		}
		else if (X > 0x4000)
		{
			X -= Terrain->BlocksX() * Block;
		}
		if (Z < -0x4000)
		{
			Z += Terrain->BlocksZ() * Block;
		}
		else if (Z > 0x4000)
		{
			Z -= Terrain->BlocksZ() * Block;
		}
	}

	int32 FWorldMapGround::BlockIndex(int32 X, int32 Z) const
	{
		const int32 Column = DivideBy8(X >> 20);
		const int32 Row = DivideBy8(Z >> 20);
		// The index is kept to 16 bits, signed, as the program shifts it into a pointer's offset.
		const int32 Index = static_cast<int16>(Row * Terrain->BlocksX() + Column);
		if (Index < 0 || Index >= Terrain->BlockCount())
		{
			bReadOutside = true;
			return 0;
		}
		return Index;
	}

	FWorldMapCell FWorldMapGround::CellAt(int32 X, int32 Z) const
	{
		FWorldMapCell Cell;
		Cell.Block = BlockIndex(X, Z);
		int32 InX = (X >> 12) & 0x7FF;
		int32 InZ = (Z >> 12) & 0x7FF;
		if (InX > 0x3FF)
		{
			InX -= 0x400;
			Cell.Quadrant |= 1;
		}
		if (InZ > 0x3FF)
		{
			InZ -= 0x400;
			Cell.Quadrant |= 2;
		}
		Cell.Column = (InX >> 4) >> 3;
		Cell.Row = (InZ >> 4) >> 3;
		return Cell;
	}

	uint32 FWorldMapGround::CornerWord(int32 X, int32 Z, int32 Corner) const
	{
		const FWorldMapCell Cell = CellAt(X, Z);
		const int32 Index = Cell.Row * FWorldMapTerrain::QuadrantSide + Cell.Column + Corner;
		return Terrain->VertexWord(Cell.Block, Cell.Quadrant, Index % FWorldMapTerrain::QuadrantSide, Index / FWorldMapTerrain::QuadrantSide);
	}

	std::array<int32, 3> FWorldMapGround::GroundNormal(int32 X, int32 Z) const
	{
		const uint32 Corner = CornerWord(X, Z, 0);
		const int32 H00 = HeightOf(Corner);
		const int32 H10 = HeightOf(CornerWord(X, Z, EastCorner));
		const int32 H01 = HeightOf(CornerWord(X, Z, SouthCorner));
		const int32 H11 = HeightOf(CornerWord(X, Z, SouthEastCorner));
		const int32 U = DivideBy8(X) & 0xFFFF;
		const int32 W = DivideBy8(Z) & 0xFFFF;
		const std::array<int32, 4>& Weights = ProgramTables->DiagonalWeights;
		FGteLongVector A{};
		FGteLongVector B{};
		if ((Corner & DiagonalBit) == 0)
		{
			if (Multiply(U - CellWidth, Weights[0]) + Multiply(-W, Weights[1]) < 0)
			{
				A = {-EdgeX, H01 - H10, -EdgeX};
				B = {-EdgeX, H00 - H10, 0};
			}
			else
			{
				A = {0, H11 - H10, -EdgeX};
				B = {-EdgeX, H01 - H10, -EdgeX};
			}
		}
		else if (Multiply(U, Weights[2]) + Multiply(-W, Weights[3]) < 0)
		{
			A = {EdgeX, H11 - H00, -EdgeX};
			B = {0, H01 - H00, -EdgeX};
		}
		else
		{
			A = {EdgeX, H10 - H00, 0};
			B = {EdgeX, H11 - H00, -EdgeX};
		}
		return VectorNormal(OuterProduct0(B, A));
	}

	int32 FWorldMapGround::Height(int32 X, int32 Z) const
	{
		const uint32 Corner = CornerWord(X, Z, 0);
		const int32 U = DivideBy8(X) & 0xFFFF;
		const int32 W = DivideBy8(Z) & 0xFFFF;
		int32 PointX = 0;
		int32 PointY = 0;
		if ((Corner & DiagonalBit) == 0)
		{
			PointX = CellWidth;
			PointY = HeightOf(CornerWord(X, Z, EastCorner)) << 12;
		}
		else
		{
			PointY = HeightOf(Corner) << 12;
		}
		const std::array<int32, 3> Normal = GroundNormal(X, Z);
		return static_cast<int32>(static_cast<uint32>(PlaneY(U, -W, PointX, PointY, 0, Normal)) << 3);
	}

	int32 FWorldMapGround::SeaHeight(int32 X, int32 Z, int32 Phase) const
	{
		const uint32 Corner = CornerWord(X, Z, 0);
		const int32 Column = static_cast<int32>(static_cast<uint32>(X) >> 19 & 7);
		const int32 Row = static_cast<int32>(static_cast<uint32>(Z) >> 19 & 7);
		const auto Wave = [Phase](int32 WaveColumn, int32 WaveRow)
		{
			const int32 Across = GteSin(Phase + WaveColumn * WaveStep);
			const int32 Down = (GteSin(SeaPhase + WaveRow * WaveStep) << 4) >> 3;
			return Multiply(Across, Down) >> 20;
		};
		const int32 North = static_cast<int16>(static_cast<int16>(Wave(Column, Row)) + HeightOf(Corner) * 8);
		const int32 NorthEast = static_cast<int16>(static_cast<int16>(Wave(Column + 1, Row)) + HeightOf(CornerWord(X, Z, EastCorner)) * 8);
		const int32 South = static_cast<int16>(static_cast<int16>(Wave(Column, Row + 1)) + HeightOf(CornerWord(X, Z, SouthCorner)) * 8);
		const int32 SouthEast = Wave(Column + 1, Row + 1) + HeightOf(CornerWord(X, Z, SouthEastCorner)) * 8;
		const int32 U = DivideBy8(X) & 0xFFFF;
		const int32 W = DivideBy8(Z) & 0xFFFF;
		const std::array<int32, 4>& Weights = ProgramTables->DiagonalWeights;
		FGteLongVector A{};
		FGteLongVector B{};
		int32 PointX = 0;
		int32 PointY = 0;
		if ((Corner & DiagonalBit) == 0)
		{
			if (Multiply(U - CellWidth, Weights[0]) + Multiply(-W, Weights[1]) < 0)
			{
				A = {-SeaEdge, South - NorthEast, -SeaEdge};
				B = {-SeaEdge, North - NorthEast, 0};
			}
			else
			{
				A = {0, SouthEast - NorthEast, -SeaEdge};
				B = {-SeaEdge, South - NorthEast, -SeaEdge};
			}
			PointX = SeaEdge;
			PointY = NorthEast;
		}
		else
		{
			if (Multiply(U, Weights[2]) + Multiply(-W, Weights[3]) >= 0)
			{
				A = {SeaEdge, NorthEast - North, 0};
				B = {SeaEdge, SouthEast - North, -SeaEdge};
			}
			else
			{
				A = {SeaEdge, SouthEast - North, -SeaEdge};
				B = {0, South - North, -SeaEdge};
			}
			PointY = North;
		}
		const std::array<int32, 3> Normal = VectorNormal(OuterProduct0(B, A));
		const int32 InX = (X >> 12) & 0x7F;
		const int32 InZ = -((Z >> 12) & 0x7F);
		return static_cast<int32>(static_cast<uint32>(PlaneY(InX, InZ, PointX, PointY, 0, Normal)) << 12);
	}

	uint16 FWorldMapGround::Attribute(int32 X, int32 Z) const
	{
		const int32 Block = BlockIndex(X, Z);
		const int32 Column = (X & 0x7FF000) >> 19;
		const int32 Row = (Z & 0x7FF000) >> 19;
		return Terrain->CellAttribute(Block, Column, Row);
	}

	int32 FWorldMapGround::TerrainType(int32 X, int32 Z) const
	{
		// The attribute is read as a signed half-word; bits 10 to 15 are clear in every file, so the shifts see none of the sign.
		const uint32 Word = static_cast<uint32>(static_cast<int32>(static_cast<int16>(Attribute(X, Z))));
		const FWorldMapSplitLine& Line = ProgramTables->SplitLines[Word & 0xF];
		const int32 U = DivideBy8(X) & 0xFFFF;
		const int32 V = DivideBy8(Z) & 0xFFFF;
		const int32 Side = (Multiply(U - Line.PointX, Line.NormalX) >> 12) + (Multiply(V - Line.PointZ, Line.NormalZ) >> 12);
		return static_cast<int32>((Side < 1 ? Word >> 7 : Word >> 4) & 7);
	}

	int32 FWorldMapGround::Region(int32 X, int32 Z) const
	{
		return static_cast<int32>(CornerWord(X, Z, 0) >> 26 & 0xF);
	}

	bool FWorldMapGround::CanTraverse(int32 TravelMode, int32 Type) const
	{
		return ProgramTables->WalkTable[static_cast<std::size_t>(TravelMode)][static_cast<std::size_t>(Type)] != 0;
	}
}
