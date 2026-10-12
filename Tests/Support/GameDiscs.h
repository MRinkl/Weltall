#pragma once

#include "Media/DiscIdentification.h"
#include "Media/Iso9660.h"

namespace Weltall::Testing
{
	/**
	 * The player's disc of a given release from the Games folder, or null when it is not there.
	 * The folder is scanned once per test process.
	 */
	const FIso9660* FindDisc(EDiscRelease Release);
}

/** Skips the current test, naming the missing disc, unless the release is present. */
#define REQUIRE_DISC(Variable, Release)                                                       \
	const ::Weltall::FIso9660* Variable = ::Weltall::Testing::FindDisc(Release);         \
	if (Variable == nullptr)                                                                  \
	{                                                                                         \
		SKIP("disc not found in Games/: " #Release);                                          \
	}
