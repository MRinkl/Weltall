#pragma once

#include "Foundation/Result.h"
#include "Media/GameFileIndex.h"

#include <string>
#include <vector>

namespace Weltall
{
	/** A sound file of the game, found by the directory and number the game names it by. */
	template <typename TFile>
	TResult<TFile> ReadSoundFile(const FCdImage& Image, const FGameFileIndex& Index, int32 Directory, int32 Number)
	{
		const FIndexRecord* Record = Index.Find(Directory, Number);
		if (Record == nullptr || !Record->IsFile())
		{
			return MakeError("the disc lacks file " + std::to_string(Number) + " of directory " + std::to_string(Directory));
		}
		TResult<std::vector<uint8>> Sectors = ReadFileSectors(Image, *Record);
		if (!Sectors)
		{
			return Sectors.Error();
		}
		Sectors->resize(static_cast<std::size_t>(Record->Size));
		return TFile::Parse(std::move(*Sectors));
	}
}
