#include "WorldMap/WorldMapDrawingTables.h"

#include "Foundation/LittleEndian.h"
#include "WorldMap/WorldMapTables.h"

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		/** Where a release's Worldmap program keeps each table, as offsets in the program. */
		struct FWorldMapDrawingLayout
		{
			EDiscRelease Release;
			std::size_t FrustumVectors;
			std::size_t QuadrantMasks;
			std::size_t SkyCorners;
			std::size_t HorizonCorners;
			std::size_t CloudTexels;
			std::size_t CloudCorners;
			std::size_t CloudMiddleCorners;
			std::size_t ParticleTexels;
			std::size_t ParticleCorners;
			std::size_t PaletteAnimations;
			std::size_t TileAnimations;
			std::size_t MinimapDots;
			std::size_t ArrowCorners;
			std::size_t FogColor;
			std::size_t ObjectFogColor;
			std::size_t ColorMatrix;
			std::size_t LightMatrix;
			std::size_t DrawModes;
		};

		constexpr std::array<FWorldMapDrawingLayout, 2> Layouts{{
			{EDiscRelease::UsaDisc1, 0x2C05C, 0x2BCB8, 0x2A790, 0x2A810, 0x2B250, 0x2B260, 0x2B2C0, 0x2B500, 0x2B550, 0x2A6F8, 0x2A760, 0x2BC04, 0x2A850, 0x2C058, 0x9EC, 0x2A650, 0x2A670, 0x2B23C},
			{EDiscRelease::UsaDisc2, 0x2C05C, 0x2BCB8, 0x2A790, 0x2A810, 0x2B250, 0x2B260, 0x2B2C0, 0x2B500, 0x2B550, 0x2A6F8, 0x2A760, 0x2BC04, 0x2A850, 0x2C058, 0x9EC, 0x2A650, 0x2A670, 0x2B23C},
		}};

		/** The vectors the planes are crossed from, 16 bytes each: up, right, the left and right edges, the top edge, down. */
		constexpr int32 FrustumVectorCount = 6;
		constexpr std::size_t VectorSize = 16;
		constexpr std::size_t CornerSize = 8;
		/** A texture animation's record: its rectangle, a word, and the address of its frames (image, passes), which end at negative passes. */
		constexpr std::size_t AnimationSize = 16;
		constexpr std::size_t AnimationFrames = 12;
		constexpr std::size_t AnimationFrameLimit = 64;

		std::array<int32, 3> Cross(const std::array<int32, 3>& A, const std::array<int32, 3>& B)
		{
			return {A[1] * B[2] - A[2] * B[1], A[2] * B[0] - A[0] * B[2], A[0] * B[1] - A[1] * B[0]};
		}

		FGteMatrix ReadRotation(std::span<const uint8> Program, std::size_t Offset)
		{
			FGteMatrix Matrix;
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				for (std::size_t Column = 0; Column < 3; ++Column)
				{
					Matrix.M[Row][Column] = ReadS16(Program, Offset + (Row * 3 + Column) * 2);
				}
			}
			return Matrix;
		}

		template <std::size_t Count>
		void ReadCorners(std::span<const uint8> Program, std::size_t Offset, std::array<FGteVector, Count>& Corners)
		{
			for (std::size_t Corner = 0; Corner < Count; ++Corner)
			{
				const std::size_t At = Offset + Corner * CornerSize;
				Corners[Corner] = {ReadS16(Program, At), ReadS16(Program, At + 2), ReadS16(Program, At + 4)};
			}
		}
	}

	namespace
	{
		template <std::size_t Count>
		std::optional<FError> ReadAnimations(std::span<const uint8> Program, std::size_t Offset, std::array<FWorldMapTextureAnimation, Count>& Animations)
		{
			for (std::size_t Index = 0; Index < Count; ++Index)
			{
				const std::size_t At = Offset + Index * AnimationSize;
				FWorldMapTextureAnimation& Animation = Animations[Index];
				Animation.Rect = {ReadS16(Program, At), ReadS16(Program, At + 2), ReadS16(Program, At + 4), ReadS16(Program, At + 6)};
				const std::size_t Frames = ReadU32(Program, At + AnimationFrames) - WorldMapProgramAddress;
				for (std::size_t Frame = 0;; ++Frame)
				{
					if (Frame == AnimationFrameLimit || Frames + Frame * 4 + 4 > Program.size())
					{
						return MakeError("a world map texture animation's frames do not end");
					}
					const FWorldMapAnimationFrame Read{ReadS16(Program, Frames + Frame * 4), ReadS16(Program, Frames + Frame * 4 + 2)};
					Animation.Frames.push_back(Read);
					if (Read.Passes < 0)
					{
						break;
					}
				}
			}
			return std::nullopt;
		}
	}

	TResult<FWorldMapDrawingTables> FWorldMapDrawingTables::Read(const FCdImage& Image, const FGameFileIndex& Index, EDiscRelease Release)
	{
		const FWorldMapDrawingLayout* Layout = nullptr;
		for (const FWorldMapDrawingLayout& Candidate : Layouts)
		{
			if (Candidate.Release == GameplayRelease(Release))
			{
				Layout = &Candidate;
			}
		}
		if (Layout == nullptr)
		{
			return MakeError("no world map drawing tables are known for this release");
		}
		const TResult<std::vector<uint8>> Unpacked = ReadWorldMapProgram(Image, Index, Release);
		if (!Unpacked)
		{
			return Unpacked.Error();
		}
		const std::span<const uint8> Program = *Unpacked;
		if (Program.size() < Layout->FrustumVectors + FrustumVectorCount * VectorSize)
		{
			return MakeError("the Worldmap program is too short to hold its drawing tables");
		}

		FWorldMapDrawingTables Tables;
		std::array<std::array<int32, 3>, FrustumVectorCount> Vectors{};
		for (std::size_t Vector = 0; Vector < Vectors.size(); ++Vector)
		{
			const std::size_t At = Layout->FrustumVectors + Vector * VectorSize;
			Vectors[Vector] = {static_cast<int32>(ReadU32(Program, At)), static_cast<int32>(ReadU32(Program, At + 4)), static_cast<int32>(ReadU32(Program, At + 8))};
		}
		// As the program's start crosses them: the left edge with up, up with the right edge, the top edge with right, right with down.
		Tables.FrustumPlanes = {Cross(Vectors[2], Vectors[0]), Cross(Vectors[0], Vectors[3]), Cross(Vectors[4], Vectors[1]), Cross(Vectors[1], Vectors[5])};
		for (std::size_t Quarter = 0; Quarter < Tables.QuadrantMasks.size(); ++Quarter)
		{
			for (std::size_t Block = 0; Block < DrawnBlocks; ++Block)
			{
				for (std::size_t Quadrant = 0; Quadrant < 4; ++Quadrant)
				{
					Tables.QuadrantMasks[Quarter][Block][Quadrant] = ReadU16(Program, Layout->QuadrantMasks + ((Quarter * DrawnBlocks + Block) * 4 + Quadrant) * 2);
				}
			}
		}
		ReadCorners(Program, Layout->SkyCorners, Tables.SkyCorners);
		ReadCorners(Program, Layout->HorizonCorners, Tables.HorizonCorners);
		ReadCorners(Program, Layout->CloudCorners, Tables.CloudCorners);
		ReadCorners(Program, Layout->CloudMiddleCorners, Tables.CloudMiddleCorners);
		for (std::size_t Layer = 0; Layer < Tables.CloudTexels.size(); ++Layer)
		{
			Tables.CloudTexels[Layer] = ReadU16(Program, Layout->CloudTexels + Layer * 2);
		}
		for (std::size_t Shape = 0; Shape < Tables.ParticleCorners.size(); ++Shape)
		{
			ReadCorners(Program, Layout->ParticleCorners + Shape * 4 * CornerSize, Tables.ParticleCorners[Shape]);
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				Tables.ParticleTexels[Shape][Corner] = ReadU16(Program, Layout->ParticleTexels + (Shape * 4 + Corner) * 2);
			}
		}
		for (std::size_t Channel = 0; Channel < 3; ++Channel)
		{
			Tables.FogColor[Channel] = Program[Layout->FogColor + Channel];
			Tables.ObjectFogColor[Channel] = Program[Layout->ObjectFogColor + Channel];
		}
		Tables.ColorMatrix = ReadRotation(Program, Layout->ColorMatrix);
		Tables.LightMatrix = ReadRotation(Program, Layout->LightMatrix);
		for (std::size_t Dot = 0; Dot < Tables.MinimapDots.size(); ++Dot)
		{
			Tables.MinimapDots[Dot] = {ReadS16(Program, Layout->MinimapDots + Dot * 4), ReadS16(Program, Layout->MinimapDots + Dot * 4 + 2)};
		}
		ReadCorners(Program, Layout->ArrowCorners, Tables.ArrowCorners);
		if (auto Error = ReadAnimations(Program, Layout->PaletteAnimations, Tables.PaletteAnimations))
		{
			return *Error;
		}
		if (auto Error = ReadAnimations(Program, Layout->TileAnimations, Tables.TileAnimations))
		{
			return *Error;
		}
		for (std::size_t Flags = 0; Flags < Tables.DrawModes.size(); ++Flags)
		{
			Tables.DrawModes[Flags] = ReadS16(Program, Layout->DrawModes + Flags * 2);
		}
		return Tables;
	}
}
