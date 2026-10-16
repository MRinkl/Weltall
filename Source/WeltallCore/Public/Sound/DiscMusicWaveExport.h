#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <filesystem>
#include <memory>

namespace Weltall
{
	class FIso9660;
	class FGameFileIndex;

	struct FDiscMusicWaveExportStatus
	{
		uint32 CompletedSongs = 0;
		uint32 TotalSongs = 0;
		int32 CurrentSong = -1;
		bool bComplete = false;
	};

	/**
	 * Converts a disc's field songs to WAVE below a music folder's originals, each named by its song's number and its
	 * sequence's key (song-07-<key>.wav), without replacing a file there. The world map plays the same sequences.
	 */
	class FDiscMusicWaveExport
	{
	public:
		WELTALLCORE_API static TResult<std::unique_ptr<FDiscMusicWaveExport>> Open(const FIso9660& Disc, const FGameFileIndex& Index,
			std::filesystem::path MusicFolder);
		WELTALLCORE_API ~FDiscMusicWaveExport();
		WELTALLCORE_API TResult<bool> Step(uint32 MaxFrames = 4096);
		WELTALLCORE_API const FDiscMusicWaveExportStatus& Status() const;

	private:
		struct FState;
		explicit FDiscMusicWaveExport(std::unique_ptr<FState> InState);
		std::unique_ptr<FState> State;
	};
}
