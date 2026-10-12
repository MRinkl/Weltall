#pragma once

#include "Foundation/Result.h"
#include "Media/Iso9660.h"

namespace Weltall
{
	/** The exact disc releases Weltall can read. Offsets inside the game differ between releases. */
	enum class EDiscRelease : uint8
	{
		UsaDisc1,
		UsaDisc2,
	};

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
