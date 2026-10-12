#include "Text/MesResident.h"

#include "Formats/Lzss.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"

#include <string>

namespace Weltall
{
	namespace
	{
		/** Where the kernel keeps them (both discs of the USA release). */
		constexpr int32 ResidentDirectory = 1;
		constexpr int32 FontFile = 6;
		constexpr int32 NameTablesFile = 7;
		constexpr uint32 BoxGlyphAddress = 0x800501D0;

		TResult<std::vector<uint8>> ReadPacked(const FCdImage& Image, const FGameFileIndex& Index, int32 Number)
		{
			const FIndexRecord* Record = Index.Find(ResidentDirectory, Number);
			if (Record == nullptr)
			{
				return MakeError("the disc lacks file " + std::to_string(Number) + " of directory 1");
			}
			const TResult<std::vector<uint8>> Sectors = ReadFileSectors(Image, *Record);
			if (!Sectors)
			{
				return Sectors.Error();
			}
			return UnpackLzss(*Sectors);
		}
	}

	TResult<FMesResident> LoadMesResident(const FCdImage& Image, const FGameFileIndex& Index, const FPsxExecutable& Executable)
	{
		const TResult<std::vector<uint8>> Font = ReadPacked(Image, Index, FontFile);
		if (!Font)
		{
			return Font.Error();
		}
		TResult<std::vector<uint8>> Names = ReadPacked(Image, Index, NameTablesFile);
		if (!Names)
		{
			return Names.Error();
		}
		const std::span<const uint8> Box = Executable.Read(BoxGlyphAddress, FMesFont::GlyphBytes);
		TResult<FMesFont> ParsedFont = FMesFont::Parse(*Font, Box);
		if (!ParsedFont)
		{
			return ParsedFont.Error();
		}
		TResult<FMesNameTables> ParsedNames = FMesNameTables::Parse(std::move(*Names));
		if (!ParsedNames)
		{
			return ParsedNames.Error();
		}
		return FMesResident{std::move(*ParsedFont), std::move(*ParsedNames)};
	}
}
