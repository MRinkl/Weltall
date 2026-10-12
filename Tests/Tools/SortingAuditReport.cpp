#include "Tools/SortingAuditReport.h"

#include <algorithm>
#include <cstdio>

namespace Weltall::Testing
{
	namespace
	{
		void WriteBitmap(const std::filesystem::path& Path, int32 Width, int32 Height, const std::vector<uint32>& Pixels)
		{
			const int32 RowBytes = (Width * 3 + 3) & ~3;
			const uint32 DataBytes = static_cast<uint32>(RowBytes * Height);
			std::vector<uint8> File(54 + DataBytes, 0);
			const auto Put32 = [&File](std::size_t At, uint32 Value)
			{
				for (std::size_t Byte = 0; Byte < 4; ++Byte)
				{
					File[At + Byte] = static_cast<uint8>(Value >> (8 * Byte));
				}
			};
			File[0] = 'B';
			File[1] = 'M';
			Put32(2, static_cast<uint32>(File.size()));
			Put32(10, 54);
			Put32(14, 40);
			Put32(18, static_cast<uint32>(Width));
			Put32(22, static_cast<uint32>(Height));
			File[26] = 1;
			File[28] = 24;
			Put32(34, DataBytes);
			for (int32 Y = 0; Y < Height; ++Y)
			{
				uint8* Row = &File[54 + static_cast<std::size_t>((Height - 1 - Y) * RowBytes)];
				for (int32 X = 0; X < Width; ++X)
				{
					const uint32 Pixel = Pixels[static_cast<std::size_t>(Y * Width + X)];
					Row[X * 3] = static_cast<uint8>(Pixel);
					Row[X * 3 + 1] = static_cast<uint8>(Pixel >> 8);
					Row[X * 3 + 2] = static_cast<uint8>(Pixel >> 16);
				}
			}
			std::ofstream(Path, std::ios::binary).write(reinterpret_cast<const char*>(File.data()), static_cast<std::streamsize>(File.size()));
		}

		std::string FileName(std::string Text)
		{
			for (char& Letter : Text)
			{
				if (Letter == ' ')
				{
					Letter = '-';
				}
			}
			return Text;
		}
	}

	void FSortingAuditReport::Add(const std::string& Label, const FSortingFraming& Framing, const FSortingFrameAudit& Audit,
		const std::function<std::string(uint32)>& Describe)
	{
		std::map<std::string, uint32> Classes;
		std::vector<std::string> ClassOf(Audit.Conflicts.size());
		for (std::size_t Number = 0; Number < Audit.Conflicts.size(); ++Number)
		{
			const FSortingConflict& Conflict = Audit.Conflicts[Number];
			if (Conflict.Pixels < LeastPixels)
			{
				continue;
			}
			const std::string Class = SortingClass(Audit.Pieces[Conflict.Over].Kind, Audit.Pieces[Conflict.Under].Kind);
			ClassOf[Number] = Class;
			Classes[Class] += Conflict.Seen;
			char Line[160];
			std::snprintf(Line, sizeof(Line), "%s\t%s\t%.2f\t%.1f\t%.1f\t", Place.c_str(), Label.c_str(), Framing.Zoom, Framing.Tilt, Framing.Turn);
			Rows << Line << Class << '\t' << Describe(Conflict.Over) << '\t' << Describe(Conflict.Under) << '\t' << Conflict.Pixels << '\t' << Conflict.Seen
				 << '\t' << static_cast<int32>(Conflict.Gap) << '\n';
		}
		if (!bPictures)
		{
			return;
		}
		for (const auto& [Class, Seen] : Classes)
		{
			FPicture& Worst = Pictures[Class];
			if (Seen <= Worst.Covered)
			{
				continue;
			}
			const int32 Width = Audit.PictureWidth;
			Worst = {Seen, Label, Width, Audit.PictureHeight, Audit.Picture, Audit.DepthPicture, std::vector<uint8>(Audit.Picture.size()), Width,
				Audit.PictureHeight, 0, 0};
			for (std::size_t Pixel = 0; Pixel < Audit.Picture.size(); ++Pixel)
			{
				const uint32 Mark = Audit.Marks[Pixel];
				if (Mark == 0 || ClassOf[Mark - 1] != Class)
				{
					continue;
				}
				const int32 X = static_cast<int32>(Pixel) % Width;
				const int32 Y = static_cast<int32>(Pixel) / Width;
				Worst.Mask[Pixel] = 1;
				Worst.Left = std::min(Worst.Left, X);
				Worst.Top = std::min(Worst.Top, Y);
				Worst.Right = std::max(Worst.Right, X + 1);
				Worst.Bottom = std::max(Worst.Bottom, Y + 1);
			}
		}
	}

	void FSortingAuditReport::WritePictures(const std::filesystem::path& Folder) const
	{
		constexpr int32 Border = 48;
		constexpr int32 LeastWidth = 360;
		constexpr int32 LeastHeight = 220;
		constexpr int32 Gap = 4;
		for (const auto& [Class, Worst] : Pictures)
		{
			const int32 Width = std::min(Worst.Width, std::max(LeastWidth, Worst.Right - Worst.Left + 2 * Border));
			const int32 Height = std::min(Worst.Height, std::max(LeastHeight, Worst.Bottom - Worst.Top + 2 * Border));
			const int32 Left = std::clamp((Worst.Left + Worst.Right - Width) / 2, 0, Worst.Width - Width);
			const int32 Top = std::clamp((Worst.Top + Worst.Bottom - Height) / 2, 0, Worst.Height - Height);
			const int32 Across = 3 * Width + 2 * Gap;
			std::vector<uint32> Sheet(static_cast<std::size_t>(Across * Height), 0x202020);
			for (int32 Y = 0; Y < Height; ++Y)
			{
				for (int32 X = 0; X < Width; ++X)
				{
					const std::size_t From = static_cast<std::size_t>((Top + Y) * Worst.Width + Left + X);
					const std::size_t To = static_cast<std::size_t>(Y * Across + X);
					const uint32 Painted = Worst.Painted[From];
					Sheet[To] = Painted;
					// Half magenta: the class's pixels stand out with the picture still seen through them.
					Sheet[To + static_cast<std::size_t>(Width + Gap)] = Worst.Mask[From] != 0 ? (Painted >> 1 & 0x7F7F7F) + 0x7F007F : Painted;
					Sheet[To + static_cast<std::size_t>(2 * (Width + Gap))] = Worst.Depth[From];
				}
			}
			WriteBitmap(Folder / (Place + "-" + FileName(Class) + "-" + Worst.Label + ".bmp"), Across, Height, Sheet);
		}
	}
}
