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
}
