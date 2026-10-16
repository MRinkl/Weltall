#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/Texture.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Weltall
{
	/** A picture of a sheet: the key it stands for and its rectangle in the sheet's PNG. */
	struct FTextureSheetCell
	{
		uint64 Key = 0;
		int32 X = 0;
		int32 Y = 0;
		int32 Width = 0;
		int32 Height = 0;

		bool operator==(const FTextureSheetCell&) const = default;
	};

	/** Where a sheet's pictures lie in it: a PNG of the sheet's size, or a whole multiple of it, holds them. */
	struct FTextureSheetLayout
	{
		int32 Width = 0;
		int32 Height = 0;
		std::vector<FTextureSheetCell> Cells;
	};

	/** A picture to put on a sheet. */
	struct FTextureSheetPicture
	{
		uint64 Key = 0;
		const FImageRgba8* Image = nullptr;
	};

	/** The widest a sheet's rows run. */
	inline constexpr int32 TextureSheetWidth = 256;

	/**
	 * Pictures laid out on one sheet in the order given, each once: rows of them left to right up to TextureSheetWidth,
	 * a transparent texel between neighbours so that a picture scaled up whole does not bleed into the next.
	 */
	WELTALLCORE_API FTextureSheetLayout LayOutTextureSheet(std::span<const FTextureSheetPicture> Pictures);

	/** The sheet's pixels, its pictures where the layout puts them and every other texel transparent. */
	WELTALLCORE_API FImageRgba8 DrawTextureSheet(const FTextureSheetLayout& Layout, std::span<const FTextureSheetPicture> Pictures);

	/**
	 * A layout as the text beside a sheet's PNG keeps it, tab-separated: "sheet", its width and height, then per picture
	 * its key (Weltall::AssetKeyName), x, y, width and height. Lines starting with # are notes.
	 */
	WELTALLCORE_API std::string WriteTextureSheetLayout(const FTextureSheetLayout& Layout);
	WELTALLCORE_API TResult<FTextureSheetLayout> ReadTextureSheetLayout(std::string_view Text);
}
