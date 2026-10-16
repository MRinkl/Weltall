#include "Graphics/TexturePictures.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int32 PageRows = 256;

		int32 TexelsInUnit(ETextureDepth Depth)
		{
			return Depth == ETextureDepth::Indexed4 ? 4 : Depth == ETextureDepth::Indexed8 ? 2 : 1;
		}

		/** The units of VRAM a page's 256 texels across take. */
		int32 PageUnits(ETextureDepth Depth)
		{
			return 256 / TexelsInUnit(Depth);
		}

		ETextureDepth DepthOf(const FVramPictureForm& Form)
		{
			return Form.Depth == 0 ? ETextureDepth::Indexed4 : Form.Depth == 1 ? ETextureDepth::Indexed8 : ETextureDepth::Direct15;
		}

		/**
		 * Where the picture's units overlap a page's columns and rows, in the page's texels, with where the whole picture
		 * lies when the page holds only part of it; false when they do not overlap.
		 */
		bool PictureInPage(const FVramRect& Rect, const FTexturePage& Page, FTextureRegion& Region)
		{
			const int32 Across = PageUnits(Page.Depth);
			const int32 Per = TexelsInUnit(Page.Depth);
			const int32 Top = std::max<int32>(Rect.Y, Page.Y);
			const int32 Bottom = std::min<int32>(Rect.Y + Rect.Height, Page.Y + PageRows);
			if (Top >= Bottom)
			{
				return false;
			}
			// A page near VRAM's right edge reaches on into its left columns, as texture reads wrap.
			for (const int32 Shift : {0, FVram::Width})
			{
				const int32 Left = std::max<int32>(Rect.X + Shift, Page.X);
				const int32 Right = std::min<int32>(Rect.X + Rect.Width + Shift, Page.X + Across);
				if (Left < Right)
				{
					Region.U = static_cast<uint8>((Left - Page.X) * Per);
					Region.V = static_cast<uint8>(Top - Page.Y);
					Region.Width = static_cast<uint16>((Right - Left) * Per);
					Region.Height = static_cast<uint16>(Bottom - Top);
					const int32 PictureU = (Rect.X + Shift - Page.X) * Per;
					const int32 PictureV = Rect.Y - Page.Y;
					if (PictureU != Region.U || PictureV != Region.V || Rect.Width * Per != Region.Width || Rect.Height != Region.Height)
					{
						Region.PictureU = static_cast<int16>(PictureU);
						Region.PictureV = static_cast<int16>(PictureV);
						Region.PictureWidth = static_cast<uint16>(Rect.Width * Per);
						Region.PictureHeight = static_cast<uint16>(Rect.Height);
					}
					return true;
				}
			}
			return false;
		}
	}

	void AddPictureRegionsInView(std::span<const FVramPicture> Pictures, uint32 View, std::vector<FTextureRegion>& Regions)
	{
		const FTexturePage Page = TextureViewPage(View);
		if (Page.Y >= FVram::Height)
		{
			return;
		}
		for (const FVramPicture& Picture : Pictures)
		{
			FTextureRegion Region{View};
			if (DepthOf(Picture.Form) == Page.Depth && PictureInPage(Picture.Rect, Page, Region))
			{
				Regions.push_back(Region);
			}
		}
	}

	void AddPictureSeeds(std::span<const FVramPicture> Pictures, std::vector<FTexturePictureSeed>& Seeds)
	{
		for (const FVramPicture& Picture : Pictures)
		{
			const FTexturePage Page{Picture.Rect.X / 64 * 64 % FVram::Width, Picture.Rect.Y / PageRows * PageRows % FVram::Height, DepthOf(Picture.Form)};
			FTextureRegion Region{PackTextureView(Page, {})};
			if (PictureInPage(Picture.Rect, Page, Region))
			{
				Seeds.push_back({Region, Picture.Form.Palette});
			}
		}
	}
}
