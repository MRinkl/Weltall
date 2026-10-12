#include "Battle/Show/BattleHudSheet.h"

#include "Formats/SectionContainer.h"
#include "Foundation/LittleEndian.h"
#include "Media/GameFileIndex.h"

#include <algorithm>
#include <utility>

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		constexpr uint32 SheetSection = 1;
		constexpr std::size_t PartSize = 0x1C;

		bool Has(std::span<const uint8> Bytes, std::size_t Offset, std::size_t Count)
		{
			return Offset <= Bytes.size() && Count <= Bytes.size() - Offset;
		}

		/** libgs's GetTPage: the texture mode, the blend, and the page holding (X, Y). */
		uint16 TexturePage(int32 Mode, int32 Blend, int32 X, int32 Y)
		{
			return static_cast<uint16>((Mode & 3) << 7 | (Blend & 3) << 5 | (Y & 0x100) >> 4 | (X & 0x3FF) >> 6 | (Y & 0x200) << 2);
		}

		uint16 Clut(int32 X, int32 Y)
		{
			return static_cast<uint16>(Y << 6 | (X >> 4 & 0x3F));
		}
	}

	TResult<FBattleHudSheet> FBattleHudSheet::Parse(std::vector<uint8> Sheet)
	{
		if (!Has(Sheet, 0, 4))
		{
			return MakeError("the battle's HUD sheet is shorter than its table");
		}
		const int32 Frames = ReadU16(Sheet, 0);
		if (!Has(Sheet, 4, 2 * static_cast<std::size_t>(Frames)))
		{
			return MakeError("the battle's HUD sheet's table runs past it");
		}
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			const std::size_t At = ReadU16(Sheet, 4 + 2 * static_cast<std::size_t>(Frame));
			if (!Has(Sheet, At, 4) || !Has(Sheet, At + 4, PartSize * static_cast<std::size_t>(std::max<int16>(ReadS16(Sheet, At), 0))))
			{
				return MakeError("a frame of the battle's HUD sheet runs past it");
			}
		}
		FBattleHudSheet Result;
		Result.Bytes = std::move(Sheet);
		Result.Frames = Frames;
		return Result;
	}

	TResult<FBattleHudSheet> FBattleHudSheet::Load(const FCdImage& Image, const FGameFileIndex& Index)
	{
		const FIndexRecord* Record = Index.Find(DataDirectory, DataFile);
		if (Record == nullptr)
		{
			return MakeError("the disc lacks the battle's data file");
		}
		const TResult<std::vector<uint8>> File = ReadFileSectors(Image, *Record);
		if (!File)
		{
			return File.Error();
		}
		TResult<std::vector<uint8>> Sheet = UnpackContainerSection(*File, SheetSection);
		if (!Sheet)
		{
			return Sheet.Error();
		}
		return Parse(std::move(*Sheet));
	}

	std::optional<int32> FBattleHudSheet::BuildParts(int32 Frame, int32 X, int32 Y, std::vector<FBattleSheetPart>& Parts) const
	{
		if (Frame < 0 || Frame >= Frames)
		{
			return std::nullopt;
		}
		const std::size_t At = ReadU16(Bytes, 4 + 2 * static_cast<std::size_t>(Frame));
		const int32 Count = ReadS16(Bytes, At);
		for (int32 Part = 0; Part < Count; ++Part)
		{
			const std::size_t Source = At + 4 + PartSize * static_cast<std::size_t>(Part);
			const uint16 U = ReadU16(Bytes, Source);
			const uint16 V = ReadU16(Bytes, Source + 2);
			const int16 Mode = ReadS16(Bytes, Source + 0x10);
			// The page is chosen by the image's place with the texels' offset added in VRAM units.
			const int32 Columns = Mode == 0 ? static_cast<int16>(U) >> 4 : static_cast<int16>(U) >> 2;
			FBattleSheetPart Record;
			Record.Clut = Clut(ReadS16(Bytes, Source + 0x12), ReadS16(Bytes, Source + 0x14));
			Record.TexturePage = TexturePage(Mode, 1, static_cast<int16>(ReadU16(Bytes, Source + 0x16) & 0xFFC0) + Columns,
				static_cast<int16>(ReadU16(Bytes, Source + 0x18) & 0xFF00) + static_cast<int16>(V));
			Record.U = static_cast<uint8>(U);
			Record.V = static_cast<uint8>(V);
			Record.Width = static_cast<uint8>(ReadU16(Bytes, Source + 4));
			Record.Height = static_cast<uint8>(ReadU16(Bytes, Source + 6));
			Record.X = static_cast<int16>(ReadS16(Bytes, Source + 8) + X);
			Record.Y = static_cast<int16>(ReadS16(Bytes, Source + 0xA) + Y);
			Parts.push_back(Record);
		}
		return Count;
	}
}
