#include "Graphics/TextureUpscale.h"

namespace Weltall::TextureUpscale
{
	namespace
	{
		int32 TexelsPerUnit(ETextureDepth Depth)
		{
			return Depth == ETextureDepth::Indexed4 ? 4 : 2;
		}

		FTextureIndices Blank(int32 Width, int32 Height)
		{
			const std::size_t Size = static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height);
			return {Width, Height, std::vector<uint8>(Size), std::vector<uint8>(Size)};
		}

		/** The edges a quarter of a texel with Edges lies on: those on the quarter's own sides. */
		uint8 QuarterEdges(uint8 Edges, int32 X, int32 Y)
		{
			const uint8 Sides = static_cast<uint8>(((X & 1) != 0 ? VramEdge::Right : VramEdge::Left) | ((Y & 1) != 0 ? VramEdge::Bottom : VramEdge::Top));
			return static_cast<uint8>(Edges & Sides);
		}

		/** A step from a texel towards Toward (-1, 0 or 1), none across an edge it lies on. */
		int32 StepWithin(int32 Toward, uint8 Edges, uint8 Low, uint8 High)
		{
			return (Toward < 0 && (Edges & Low) != 0) || (Toward > 0 && (Edges & High) != 0) ? 0 : Toward;
		}

		/** The index a step from the texel at (X, Y) towards (TowardX, TowardY) reaches, none crossing an edge the texel lies on. */
		uint8 NearOf(const FTextureIndices& Image, int32 X, int32 Y, int32 TowardX, int32 TowardY)
		{
			const uint8 Edges = Image.EdgesAt(X, Y);
			return Image.At(X + StepWithin(TowardX, Edges, VramEdge::Left, VramEdge::Right), Y + StepWithin(TowardY, Edges, VramEdge::Top, VramEdge::Bottom));
		}

		/** Whether the texel's two neighbours beside its corner towards (SideX, SideY) share an index the two across from them do not. */
		bool IsStep(const FTextureIndices& Image, int32 X, int32 Y, int32 SideX, int32 SideY)
		{
			const uint8 Across = NearOf(Image, X, Y, SideX, 0);
			return Across == NearOf(Image, X, Y, 0, SideY) && Across != NearOf(Image, X, Y, -SideX, 0) && Across != NearOf(Image, X, Y, 0, -SideY);
		}

		/** IsStep, with the texel's own index behind the corner: the two neighbours are a line passing between. */
		bool IsLineStep(const FTextureIndices& Image, int32 X, int32 Y, int32 SideX, int32 SideY)
		{
			return IsStep(Image, X, Y, SideX, SideY) && NearOf(Image, X, Y, SideX, SideY) == Image.At(X, Y);
		}

		void DoubleOver(const FTextureIndices& Image, const FTexelRect& Rect, FTextureIndices& Into)
		{
			for (int32 Y = Rect.Y; Y < Rect.Y + Rect.Height; ++Y)
			{
				for (int32 X = Rect.X; X < Rect.X + Rect.Width; ++X)
				{
					const std::size_t At = Into.Place(X, Y);
					Into.Indices[At] = DoubledTexel(Image, X, Y);
					Into.Edges[At] = QuarterEdges(Image.EdgesAt(X >> 1, Y >> 1), X, Y);
				}
			}
		}
	}

	FTextureIndices ReadIndices(const FVram& Vram, ETextureDepth Depth)
	{
		const int32 PerUnit = TexelsPerUnit(Depth);
		const int32 Bits = 16 / PerUnit;
		const uint32 Mask = (1u << Bits) - 1;
		FTextureIndices Image = Blank(FVram::Width * PerUnit, FVram::Height);
		for (int32 Y = 0; Y < FVram::Height; ++Y)
		{
			for (int32 X = 0; X < Image.Width; ++X)
			{
				const uint32 Unit = Vram.At(X / PerUnit, Y);
				const std::size_t At = Image.Place(X, Y);
				Image.Indices[At] = static_cast<uint8>(Unit >> (X % PerUnit * Bits) & Mask);
				// A unit's left edge is its first texel's, its right edge its last's.
				const uint8 Sides = static_cast<uint8>(VramEdge::Top | VramEdge::Bottom | (X % PerUnit == 0 ? VramEdge::Left : 0)
					| (X % PerUnit == PerUnit - 1 ? VramEdge::Right : 0));
				Image.Edges[At] = static_cast<uint8>(Vram.EdgesAt(X / PerUnit, Y) & Sides);
			}
		}
		return Image;
	}

	uint8 DoubledTexel(const FTextureIndices& Image, int32 X, int32 Y)
	{
		const int32 CentreX = X >> 1;
		const int32 CentreY = Y >> 1;
		// The quarter faces the neighbours on its own side across and down.
		const int32 SideX = (X & 1) != 0 ? 1 : -1;
		const int32 SideY = (Y & 1) != 0 ? 1 : -1;
		const uint8 Centre = Image.At(CentreX, CentreY);
		if (!IsStep(Image, CentreX, CentreY, SideX, SideY))
		{
			return Centre;
		}
		const uint8 Across = NearOf(Image, CentreX, CentreY, SideX, 0);
		if (NearOf(Image, CentreX, CentreY, SideX, SideY) == Centre)
		{
			// The texel, the one behind the corner and the two neighbours make a 2x2 checker: the neighbours' line passes
			// between, and takes the corner unless the texel's line has the same claim from their side, where two lines
			// cross and the higher index takes it (both taking it would swap the four quarters into a checker of their own).
			const bool bCrossing = IsLineStep(Image, CentreX + SideX, CentreY, -SideX, SideY) || IsLineStep(Image, CentreX, CentreY + SideY, SideX, -SideY);
			return !bCrossing || Across > Centre ? Across : Centre;
		}
		const bool bStaircase = NearOf(Image, CentreX, CentreY, -SideX, SideY) == Centre || NearOf(Image, CentreX, CentreY, SideX, -SideY) == Centre;
		return bStaircase && Across > Centre ? Across : Centre;
	}

	FTextureIndices Double(const FTextureIndices& Image)
	{
		FTextureIndices Doubled = Blank(Image.Width * 2, Image.Height * 2);
		DoubleOver(Image, {0, 0, Doubled.Width, Doubled.Height}, Doubled);
		return Doubled;
	}

	FTextureIndices Upscale(const FVram& Vram, ETextureDepth Depth)
	{
		FTextureIndices Image = ReadIndices(Vram, Depth);
		for (int32 Scale = 1; Scale < Factor; Scale *= 2)
		{
			Image = Double(Image);
		}
		return Image;
	}

	std::vector<FVramRect> RectsToRedo(const FVramTiles& Changed)
	{
		// A texel's doubling reads the texels next to it, and the upscaling the doubled texels next to its own: a
		// change reaches less than two texels, under a unit across and two rows down, so a tile's neighbours cover it.
		FVramTiles Grown{};
		for (int32 TileY = 0; TileY < FVram::TilesDown; ++TileY)
		{
			const uint64 Row = Changed[static_cast<std::size_t>(TileY)];
			// The row's tiles and their neighbours, the first's and the last's around VRAM's edge.
			const uint64 Wide = Row | Row << 1 | Row >> 1 | Row << (FVram::TilesAcross - 1) | Row >> (FVram::TilesAcross - 1);
			for (const int32 Near : {TileY - 1, TileY, TileY + 1})
			{
				Grown[static_cast<std::size_t>((Near + FVram::TilesDown) % FVram::TilesDown)] |= Wide;
			}
		}
		std::vector<FVramRect> Rects;
		std::vector<std::size_t> Open;
		for (int32 TileY = 0; TileY < FVram::TilesDown; ++TileY)
		{
			std::vector<std::size_t> StillOpen;
			const uint64 Row = Grown[static_cast<std::size_t>(TileY)];
			for (int32 First = 0; First < FVram::TilesAcross;)
			{
				if ((Row >> First & 1) == 0)
				{
					++First;
					continue;
				}
				int32 End = First;
				while (End < FVram::TilesAcross && (Row >> End & 1) != 0)
				{
					++End;
				}
				const FVramRect Run{static_cast<int16>(First * FVram::TileWidth), static_cast<int16>(TileY * FVram::TileHeight),
					static_cast<int16>((End - First) * FVram::TileWidth), static_cast<int16>(FVram::TileHeight)};
				std::size_t Joined = Rects.size();
				for (const std::size_t Candidate : Open)
				{
					if (Rects[Candidate].X == Run.X && Rects[Candidate].Width == Run.Width)
					{
						Joined = Candidate;
					}
				}
				if (Joined < Rects.size())
				{
					Rects[Joined].Height = static_cast<int16>(Rects[Joined].Height + FVram::TileHeight);
				}
				else
				{
					Rects.push_back(Run);
				}
				StillOpen.push_back(Joined);
				First = End;
			}
			Open = std::move(StillOpen);
		}
		return Rects;
	}

	FTexelRect TexelsOver(const FVramRect& Rect, ETextureDepth Depth, int32 Scale)
	{
		const int32 Across = TexelsPerUnit(Depth) * Scale;
		return {Rect.X * Across, Rect.Y * Scale, Rect.Width * Across, Rect.Height * Scale};
	}

	void Redo(const FVram& Vram, ETextureDepth Depth, std::span<const FVramRect> Rects, FTextureIndices& Doubled, FTextureIndices& Upscaled)
	{
		const FTextureIndices Read = ReadIndices(Vram, Depth);
		for (const FVramRect& Rect : Rects)
		{
			DoubleOver(Read, TexelsOver(Rect, Depth, 2), Doubled);
		}
		for (const FVramRect& Rect : Rects)
		{
			DoubleOver(Doubled, TexelsOver(Rect, Depth, Factor), Upscaled);
		}
	}
}
