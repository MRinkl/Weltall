#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Weltall
{
	/** One track of a CUE sheet, with its data file resolved relative to the sheet. */
	struct FCueTrack
	{
		int32 Number = 0;
		std::string Mode;
		std::filesystem::path File;
		uint32 SectorSize = 0;
		/** Start of the track's INDEX 01 within its file, in sectors (frames). */
		uint32 StartFrame = 0;

		bool IsAudio() const { return Mode == "AUDIO"; }
	};

	/** Parses the text of a CUE sheet; relative file names are resolved against SheetDirectory. */
	WELTALLCORE_API TResult<std::vector<FCueTrack>> ParseCueSheet(std::string_view Text, const std::filesystem::path& SheetDirectory);
}
