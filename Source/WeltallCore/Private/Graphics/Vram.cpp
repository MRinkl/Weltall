#include "Graphics/Vram.h"

#include <algorithm>
#include <atomic>

namespace Weltall
{
	namespace
	{
		std::atomic<uint32> LineagesMade{0};
	}

	FVram::FVram()
		: Units(static_cast<std::size_t>(Width) * Rows, 0), Edges(static_cast<std::size_t>(Width) * Rows, 0),
		  LineageNumber(++LineagesMade), TileRevisions(static_cast<std::size_t>(TilesAcross) * TilesDown, 0)
	{
	}

	void FVram::Upload(const FVramImage& Image)
	{
		++Uploads;
		if (bTrackingWrites)
		{
			Writes.push_back(Image.Rect);
		}
		const int32 RectWidth = Image.Rect.Width;
		const int32 RectHeight = Image.Rect.Height;
		for (int32 Row = 0; Row < RectHeight; ++Row)
		{
			const int32 Y = (Image.Rect.Y + Row) & (Height - 1);
			uint8 RowEdges = 0;
			if (Image.bSeparateTexture)
			{
				RowEdges = static_cast<uint8>((Row == 0 ? VramEdge::Top : 0) | (Row == RectHeight - 1 ? VramEdge::Bottom : 0));
			}
			for (int32 Column = 0; Column < RectWidth; ++Column)
			{
				const int32 X = (Image.Rect.X + Column) & (Width - 1);
				const std::size_t At = static_cast<std::size_t>(Y * Width + X);
				Units[At] = Image.Units[static_cast<std::size_t>(Row * RectWidth + Column)];
				uint8 UnitEdges = RowEdges;
				if (Image.bSeparateTexture)
				{
					UnitEdges = static_cast<uint8>(UnitEdges | (Column == 0 ? VramEdge::Left : 0) | (Column == RectWidth - 1 ? VramEdge::Right : 0));
				}
				Edges[At] = UnitEdges;
			}
		}
		// Each tile the rectangle touches once, stepping from tile edge to tile edge as the wrapped coordinates cross them.
		for (int32 Row = 0; Row < std::min(RectHeight, Height);)
		{
			const int32 Y = (Image.Rect.Y + Row) & (Height - 1);
			for (int32 Column = 0; Column < std::min(RectWidth, Width);)
			{
				const int32 X = (Image.Rect.X + Column) & (Width - 1);
				TileRevisions[static_cast<std::size_t>(Y / TileHeight * TilesAcross + X / TileWidth)] = Uploads;
				Column += TileWidth - X % TileWidth;
			}
			Row += TileHeight - Y % TileHeight;
		}
	}

	void FVram::UploadToPortRows(const FVramImage& Image)
	{
		++Uploads;
		const int32 RectWidth = Image.Rect.Width;
		const int32 RectHeight = Image.Rect.Height;
		for (int32 Row = 0; Row < RectHeight; ++Row)
		{
			const int32 Y = Image.Rect.Y + Row;
			if (Y < Height || Y >= Rows)
			{
				continue;
			}
			for (int32 Column = 0; Column < RectWidth; ++Column)
			{
				const std::size_t At = static_cast<std::size_t>(Y * Width + ((Image.Rect.X + Column) & (Width - 1)));
				Units[At] = Image.Units[static_cast<std::size_t>(Row * RectWidth + Column)];
				Edges[At] = Image.bSeparateTexture
					? static_cast<uint8>((Row == 0 ? VramEdge::Top : 0) | (Row == RectHeight - 1 ? VramEdge::Bottom : 0) | (Column == 0 ? VramEdge::Left : 0)
						| (Column == RectWidth - 1 ? VramEdge::Right : 0))
					: 0;
			}
		}
	}

	FVramTiles FVram::TilesChangedSince(uint32 Earlier) const
	{
		FVramTiles Tiles{};
		for (int32 TileY = 0; TileY < TilesDown; ++TileY)
		{
			for (int32 TileX = 0; TileX < TilesAcross; ++TileX)
			{
				if (TileRevisions[static_cast<std::size_t>(TileY * TilesAcross + TileX)] > Earlier)
				{
					Tiles[static_cast<std::size_t>(TileY)] |= uint64{1} << TileX;
				}
			}
		}
		return Tiles;
	}
}
