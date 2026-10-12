#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;

	namespace BattlingFiles
	{
		/** The program's own directory and the directories of its data and its gears. */
		inline constexpr int32 ProgramDirectory = 1;
		inline constexpr int32 ProgramFile = 13;
		inline constexpr int32 DataDirectory = 48;
		inline constexpr int32 GearDirectory = 49;

		/** One of Battling's LZSS-packed files, unpacked. */
		TResult<std::vector<uint8>> ReadPacked(const FCdImage& Image, const FGameFileIndex& Index, int32 Directory, int32 Number);
	}
}
