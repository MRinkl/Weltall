#pragma once

#include "Foundation/Result.h"
#include "Media/Iso9660.h"

#include <string_view>

namespace Weltall
{
	/** The exact disc releases Weltall can read. Offsets inside the game differ between releases. */
	enum class EDiscRelease : uint8
	{
		UsaDisc1,
		UsaDisc2,
		UndubDisc1,
		UndubDisc2,
		JapanDisc1,
		JapanDisc2,
	};

	/** Stable directory name for a disc's locally extracted assets. */
	WELTALLCORE_API std::string_view DiscReleaseId(EDiscRelease Release);
	/** One or two, independently of the disc's language. */
	WELTALLCORE_API uint8 DiscNumberOfRelease(EDiscRelease Release);
	/** Whether the release provides the English game data the port plays. */
	WELTALLCORE_API bool IsPlayableRelease(EDiscRelease Release);
	/** The compatible original layout for a playable release. */
	WELTALLCORE_API EDiscRelease GameplayRelease(EDiscRelease Release);

	struct FDiscIdentity
	{
		EDiscRelease Release;
		/** 1 or 2: which of the game's two discs this is. */
		uint8 DiscNumber;
		const char* Title;
	};

	/**
	 * Recognizes a disc by its boot executable: the file name gives the disc, the file's hash gives
	 * the exact release. Different rips of the same disc therefore match, whatever their other files.
	 */
	WELTALLCORE_API TResult<FDiscIdentity> IdentifyDisc(const FIso9660& Disc);
}
