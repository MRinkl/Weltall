#include "Formats/VramImageList.h"

#include "Foundation/LittleEndian.h"

#include <algorithm>
#include <utility>

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		constexpr uint32 ImageBlock = 0x1100;
		constexpr uint32 ClutBlock = 0x1101;
		constexpr std::size_t BlockHeader = 16;
	}

	TResult<std::vector<FVramImage>> ReadVramImageList(std::span<const uint8> List)
	{
		if (List.size() < 4)
		{
			return MakeError("an image list is cut short");
		}
		const uint32 Count = ReadU32(List, 0);
		std::size_t At = 4 + 4 * static_cast<std::size_t>(Count);
		std::vector<FVramImage> Images;
		for (uint32 Block = 0; Block < Count; ++Block)
		{
			if (List.size() < At + BlockHeader)
			{
				return MakeError("an image list's block is cut short");
			}
			const uint32 Kind = ReadU32(List, At);
			if (Kind != ImageBlock && Kind != ClutBlock)
			{
				break;
			}
			FVramImage Image;
			Image.Rect = {static_cast<int16>(ReadS16(List, At + 4) + ReadS16(List, At + 8)),
				static_cast<int16>(ReadS16(List, At + 6) + ReadS16(List, At + 10)), ReadS16(List, At + 12), ReadS16(List, At + 14)};
			const std::size_t Units = static_cast<std::size_t>(std::max(0, Image.Rect.Width * Image.Rect.Height));
			if (List.size() < At + BlockHeader + 2 * Units)
			{
				return MakeError("an image list's units are cut short");
			}
			Image.Units.resize(Units);
			for (std::size_t Unit = 0; Unit < Units; ++Unit)
			{
				Image.Units[Unit] = ReadU16(List, At + BlockHeader + 2 * Unit);
			}
			At += BlockHeader + 2 * Units;
			Images.push_back(std::move(Image));
		}
		return Images;
	}
}
