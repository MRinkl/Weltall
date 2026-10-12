#include "Graphics/SpriteFile.h"

#include "Foundation/LittleEndian.h"

#include <string>

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		constexpr uint32 SectionCount = 3;
		constexpr uint32 HeaderBytes = 0x14;
		constexpr uint8 CommandBit = 0x80;
		constexpr uint8 AnchorCommandBit = 0x40;
		constexpr uint8 WidePlacementBit = 0x80;
		constexpr uint8 KernelCellBit = 0x10;

		/** Frame lists per facing direction mode: one, four (three stored), eight (five stored); mode 3 reads one. */
		constexpr uint32 DirectionLists[4] = {1, 3, 5, 1};

		FError Outside(const std::string& What)
		{
			return MakeError("sprite file: " + What + " lies outside the file");
		}

		/** The length of a command byte with its operands. */
		uint32 CommandLength(uint8 Command)
		{
			if ((Command & AnchorCommandBit) != 0)
			{
				return 1 + ((Command & 0x20) != 0 ? 2 : 0) + ((Command & 0x10) != 0 ? 1 : 0);
			}
			return 1 + (Command & 1) + (Command >> 1 & 1);
		}

		/** Walks a frame's placement stream from At; false when it runs past the file. */
		bool PlacementsFit(std::span<const uint8> Bytes, uint32 At, uint32 Parts, bool bWide)
		{
			for (uint32 Part = 0; Part < Parts; ++Part)
			{
				while (At < Bytes.size() && (Bytes[At] & CommandBit) != 0)
				{
					At += CommandLength(Bytes[At]);
				}
				At += bWide ? 5 : 3;
				if (At > Bytes.size())
				{
					return false;
				}
			}
			return true;
		}

		bool CellFits(std::span<const uint8> Bytes, uint32 Cell, bool bResident)
		{
			if (bResident)
			{
				return Cell < Bytes.size() && Cell + ((Bytes[Cell] & KernelCellBit) != 0 ? 6 : 5) <= Bytes.size();
			}
			if (Cell + 4 > Bytes.size())
			{
				return false;
			}
			const uint32 Units = (Bytes[Cell + 2] & 1) != 0 ? Bytes[Cell] >> 1 : Bytes[Cell] >> 2;
			return Cell + 4 + Units * 2 * Bytes[Cell + 1] <= Bytes.size();
		}
	}

	TResult<FSpriteFile> FSpriteFile::Parse(std::span<const uint8> Bytes)
	{
		if (Bytes.size() < HeaderBytes || ReadU32(Bytes, 0) != SectionCount)
		{
			return MakeError("not a sprite file: it does not start with its three sections");
		}
		return ParseTables(Bytes);
	}

	TResult<FSpriteFile> FSpriteFile::ParseWithSections(std::span<const uint8> Bytes)
	{
		if (Bytes.size() < HeaderBytes || ReadU32(Bytes, 0) < SectionCount)
		{
			return MakeError("not a sprite file: it does not start with at least three sections");
		}
		return ParseTables(Bytes);
	}

	TResult<FSpriteFile> FSpriteFile::ParseTables(std::span<const uint8> Bytes)
	{
		FSpriteFile File;
		File.Own.Animations = ReadU32(Bytes, 4);
		File.Own.Frames = ReadU32(Bytes, 8);
		File.Own.Palettes = ReadU32(Bytes, 12);
		const FSpriteTables& Own = File.Own;
		const std::size_t Size = Bytes.size();
		if (Own.Animations + 2 > Size || Own.Frames + 2 > Size || Own.Palettes + 4 > Size)
		{
			return Outside("a section");
		}
		File.Bytes.assign(Bytes.begin(), Bytes.end());

		if (File.AnimationCount(Own) == 0 || File.ByteTable(Own) > Size)
		{
			return Outside("the animation table");
		}
		for (int32 Animation = 0; Animation < File.AnimationCount(Own); ++Animation)
		{
			const uint32 Header = File.AnimationHeader(Own, Animation);
			const uint32 Lists = DirectionLists[File.Word(Header) & 3];
			if (Header + 4 + 2 * Lists > Size || Header + 2 + File.Word(Header + 2) >= Size)
			{
				return Outside("animation " + std::to_string(Animation));
			}
			for (uint32 List = 0; List < Lists; ++List)
			{
				const uint32 At = Header + 4 + 2 * List;
				if (At + File.Word(At) >= Size)
				{
					return Outside("animation " + std::to_string(Animation) + "'s frame list");
				}
			}
		}

		const bool bResident = File.IsResident(Own);
		if (Own.Frames + 2 + 2 * static_cast<uint32>(File.FrameCount(Own)) > Size)
		{
			return Outside("the frame table");
		}
		for (int32 Number = 1; Number <= File.FrameCount(Own); ++Number)
		{
			const std::string Name = "frame " + std::to_string(Number);
			const uint32 At = File.Frame(Own, Number);
			const uint32 EntryBytes = bResident ? 2 : 4;
			const uint32 EntriesAt = At + (bResident ? 4 : 6);
			if (EntriesAt > Size)
			{
				return Outside(Name);
			}
			const uint32 Parts = Bytes[At] & 0x3Fu;
			if (static_cast<int32>(Parts) > File.MaxParts(Own))
			{
				return MakeError("sprite file: " + Name + " has more parts than the file's frames may have");
			}
			if (EntriesAt + EntryBytes * Parts > Size)
			{
				return Outside(Name + "'s parts");
			}
			for (uint32 Part = 0; Part < Parts; ++Part)
			{
				const uint32 Entry = ReadU16(Bytes, EntriesAt + EntryBytes * Part);
				if (!CellFits(Bytes, Own.Frames + (bResident ? Entry : 4 * Entry), bResident))
				{
					return Outside(Name + "'s cell " + std::to_string(Part));
				}
			}
			if (!PlacementsFit(Bytes, EntriesAt + EntryBytes * Parts, Parts, (Bytes[At] & WidePlacementBit) != 0))
			{
				return Outside(Name + "'s placements");
			}
		}
		return File;
	}
}
