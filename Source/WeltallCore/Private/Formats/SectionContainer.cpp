#include "Formats/SectionContainer.h"

#include "Formats/Lzss.h"
#include "Foundation/LittleEndian.h"

#include <string>

namespace Weltall
{
	TResult<std::vector<uint8>> UnpackContainerSection(std::span<const uint8> File, uint32 Number)
	{
		if (Number == 0 || File.size() < 4 || LittleEndian::ReadU32(File, 0) < Number || File.size() < 4 + 4 * static_cast<std::size_t>(Number))
		{
			return MakeError("the file lacks section " + std::to_string(Number));
		}
		const uint32 Offset = LittleEndian::ReadU32(File, 4 * static_cast<std::size_t>(Number));
		if (Offset >= File.size())
		{
			return MakeError("section " + std::to_string(Number) + " starts past the file's end");
		}
		return UnpackLzss(File.subspan(Offset));
	}

	std::optional<uint32> ContainerSectionCount(std::span<const uint8> File)
	{
		constexpr uint32 MostSections = 64;
		if (File.size() < 4)
		{
			return std::nullopt;
		}
		const uint32 Count = LittleEndian::ReadU32(File, 0);
		const std::size_t Header = 4 + 4 * static_cast<std::size_t>(Count);
		if (Count == 0 || Count > MostSections || Header > File.size())
		{
			return std::nullopt;
		}
		std::size_t Previous = Header;
		for (uint32 Number = 1; Number <= Count; ++Number)
		{
			const std::size_t Offset = LittleEndian::ReadU32(File, 4 * static_cast<std::size_t>(Number));
			if (Offset < Previous || Offset >= File.size())
			{
				return std::nullopt;
			}
			Previous = Offset;
		}
		return Count;
	}
}
