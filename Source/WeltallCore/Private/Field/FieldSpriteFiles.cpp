#include "Field/FieldSpriteFiles.h"

#include "Formats/Lzss.h"
#include "Foundation/LittleEndian.h"
#include "Media/GameFileIndex.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		/** A sprite file's first word: its section count. A packed file starts with its unpacked size instead. */
		constexpr uint32 SpriteFileMark = 3;
	}

	TResult<std::vector<FSpriteFile>> ParseFieldSpriteFiles(std::span<const uint8> Section)
	{
		if (Section.size() < 4)
		{
			return MakeError("sprite section is shorter than its count");
		}
		const uint32 Count = ReadU32(Section, 0);
		if (4 + std::size_t{Count} * 4 > Section.size())
		{
			return MakeError("sprite section names " + std::to_string(Count) + " files its " + std::to_string(Section.size()) + " bytes cannot hold");
		}
		std::vector<uint32> Starts(Count);
		for (uint32 File = 0; File < Count; ++File)
		{
			Starts[File] = ReadU32(Section, 4 + 4 * std::size_t{File});
		}
		std::vector<uint32> Ends = Starts;
		Ends.push_back(static_cast<uint32>(Section.size()));
		std::sort(Ends.begin(), Ends.end());
		std::vector<FSpriteFile> Files;
		Files.reserve(Count);
		for (uint32 File = 0; File < Count; ++File)
		{
			const uint32 Start = Starts[File];
			if (Start >= Section.size())
			{
				return MakeError("sprite file " + std::to_string(File) + " starts past its section");
			}
			const uint32 End = *std::upper_bound(Ends.begin(), Ends.end(), Start);
			auto Parsed = FSpriteFile::Parse(Section.subspan(Start, End - Start));
			if (!Parsed)
			{
				return MakeError("sprite file " + std::to_string(File) + ": " + Parsed.Error().Message);
			}
			Files.push_back(std::move(*Parsed));
		}
		return Files;
	}

	TResult<FSpriteFile> ReadSpriteFile(const FCdImage& Image, const FGameFileIndex& Index, int32 Directory, int32 Number)
	{
		const FIndexRecord* Record = Index.Find(Directory, Number);
		if (Record == nullptr)
		{
			return MakeError("sprite file " + std::to_string(Directory) + "/" + std::to_string(Number) + " is not in the file index");
		}
		auto Sectors = ReadFileSectors(Image, *Record);
		if (!Sectors)
		{
			return Sectors.Error();
		}
		if (Sectors->size() >= 4 && ReadU32(*Sectors, 0) == SpriteFileMark)
		{
			return FSpriteFile::Parse(*Sectors);
		}
		auto Unpacked = UnpackLzss(*Sectors);
		if (!Unpacked)
		{
			return Unpacked.Error();
		}
		return FSpriteFile::Parse(*Unpacked);
	}
}
