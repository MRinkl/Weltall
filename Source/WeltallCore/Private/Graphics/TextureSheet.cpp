#include "Graphics/TextureSheet.h"

#include "Graphics/TextureRegion.h"

#include <algorithm>
#include <charconv>
#include <optional>
#include <unordered_set>

namespace Weltall
{
	namespace
	{
		/** The transparent texels between neighbouring pictures, across and down. */
		constexpr int32 Gap = 1;
		constexpr int32 MostTexels = 4096;

		std::vector<std::string_view> Fields(std::string_view Line)
		{
			std::vector<std::string_view> Split;
			while (true)
			{
				const std::size_t Tab = Line.find('\t');
				Split.push_back(Line.substr(0, Tab));
				if (Tab == std::string_view::npos)
				{
					return Split;
				}
				Line.remove_prefix(Tab + 1);
			}
		}

		bool ReadNumber(std::string_view Field, int32& Value)
		{
			const auto [End, Error] = std::from_chars(Field.data(), Field.data() + Field.size(), Value);
			return Error == std::errc() && End == Field.data() + Field.size();
		}
	}

	FTextureSheetLayout LayOutTextureSheet(std::span<const FTextureSheetPicture> Pictures)
	{
		FTextureSheetLayout Layout;
		std::unordered_set<uint64> Placed;
		int32 X = 0;
		int32 Y = 0;
		int32 RowHeight = 0;
		for (const FTextureSheetPicture& Picture : Pictures)
		{
			if (!Placed.insert(Picture.Key).second)
			{
				continue;
			}
			const int32 Width = Picture.Image->Width;
			const int32 Height = Picture.Image->Height;
			if (X > 0 && X + Width > TextureSheetWidth)
			{
				X = 0;
				Y += RowHeight + Gap;
				RowHeight = 0;
			}
			Layout.Cells.push_back({Picture.Key, X, Y, Width, Height});
			Layout.Width = std::max(Layout.Width, X + Width);
			Layout.Height = std::max(Layout.Height, Y + Height);
			RowHeight = std::max(RowHeight, Height);
			X += Width + Gap;
		}
		return Layout;
	}

	FImageRgba8 DrawTextureSheet(const FTextureSheetLayout& Layout, std::span<const FTextureSheetPicture> Pictures)
	{
		FImageRgba8 Sheet{Layout.Width, Layout.Height, std::vector<uint8>(static_cast<std::size_t>(Layout.Width) * Layout.Height * 4, 0)};
		for (const FTextureSheetCell& Cell : Layout.Cells)
		{
			const auto Picture = std::find_if(Pictures.begin(), Pictures.end(), [&Cell](const FTextureSheetPicture& Candidate) { return Candidate.Key == Cell.Key; });
			if (Picture == Pictures.end())
			{
				continue;
			}
			for (int32 Row = 0; Row < Cell.Height; ++Row)
			{
				const uint8* From = Picture->Image->Pixels.data() + static_cast<std::size_t>(Row) * Cell.Width * 4;
				uint8* Into = Sheet.Pixels.data() + (static_cast<std::size_t>(Cell.Y + Row) * Layout.Width + Cell.X) * 4;
				std::copy_n(From, static_cast<std::size_t>(Cell.Width) * 4, Into);
			}
		}
		return Sheet;
	}

	std::string WriteTextureSheetLayout(const FTextureSheetLayout& Layout)
	{
		std::string Text = "# The pictures of the PNG beside this file: each one's key, then its x, y, width and height.\n";
		Text += "sheet\t" + std::to_string(Layout.Width) + "\t" + std::to_string(Layout.Height) + "\n";
		for (const FTextureSheetCell& Cell : Layout.Cells)
		{
			Text += AssetKeyName(Cell.Key) + "\t" + std::to_string(Cell.X) + "\t" + std::to_string(Cell.Y) + "\t" + std::to_string(Cell.Width) + "\t" +
				std::to_string(Cell.Height) + "\n";
		}
		return Text;
	}

	TResult<FTextureSheetLayout> ReadTextureSheetLayout(std::string_view Text)
	{
		FTextureSheetLayout Layout;
		bool bSized = false;
		std::unordered_set<uint64> Keys;
		int32 Number = 0;
		while (!Text.empty())
		{
			const std::size_t End = Text.find('\n');
			std::string_view Line = Text.substr(0, End);
			Text.remove_prefix(End == std::string_view::npos ? Text.size() : End + 1);
			++Number;
			if (!Line.empty() && Line.back() == '\r')
			{
				Line.remove_suffix(1);
			}
			if (Line.empty() || Line.front() == '#')
			{
				continue;
			}
			const std::vector<std::string_view> Split = Fields(Line);
			const std::string Where = "line " + std::to_string(Number);
			if (!bSized)
			{
				if (Split.size() != 3 || Split[0] != "sheet" || !ReadNumber(Split[1], Layout.Width) || !ReadNumber(Split[2], Layout.Height) ||
					Layout.Width <= 0 || Layout.Height <= 0 || Layout.Width > MostTexels || Layout.Height > MostTexels)
				{
					return MakeError(Where + ": a sheet starts with \"sheet\", its width and its height");
				}
				bSized = true;
				continue;
			}
			FTextureSheetCell Cell;
			const std::optional<uint64> Key = Split.size() == 5 ? AssetKeyOfName(Split[0]) : std::nullopt;
			if (!Key || Split[0].size() != 16 || !ReadNumber(Split[1], Cell.X) || !ReadNumber(Split[2], Cell.Y) || !ReadNumber(Split[3], Cell.Width) ||
				!ReadNumber(Split[4], Cell.Height))
			{
				return MakeError(Where + ": a picture is its key, x, y, width and height");
			}
			Cell.Key = *Key;
			if (Cell.X < 0 || Cell.Y < 0 || Cell.Width <= 0 || Cell.Height <= 0 || Cell.X + Cell.Width > Layout.Width || Cell.Y + Cell.Height > Layout.Height)
			{
				return MakeError(Where + ": the picture lies outside the sheet");
			}
			if (Keys.insert(Cell.Key).second)
			{
				Layout.Cells.push_back(Cell);
			}
		}
		if (!bSized)
		{
			return MakeError("the sheet's size is missing");
		}
		return Layout;
	}
}
