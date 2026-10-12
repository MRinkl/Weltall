#pragma once

#include "Chd/ChdHeader.h"
#include "Chd/ChdTypes.h"

#include <vector>

namespace Chd
{
	/** The metadata tags that say what kind of disc a CHD holds. */
	namespace MetadataTag
	{
		/** A DVD; its data holds nothing further. */
		inline constexpr uint32 Dvd = MakeTag('D', 'V', 'D', ' ');
		/** A CD track, in text: "TRACK:1 TYPE:MODE2_RAW SUBTYPE:NONE FRAMES:1234 PREGAP:0 PGTYPE:MODE1 PGSUB:RW POSTGAP:0". */
		inline constexpr uint32 CdTrack = MakeTag('C', 'H', 'T', '2');
		/** A CD track as older chdman wrote it, without the pregap: "TRACK:1 TYPE:MODE1 SUBTYPE:NONE FRAMES:1234". */
		inline constexpr uint32 CdTrackOld = MakeTag('C', 'H', 'T', 'R');
		/** A GD-ROM track, the Dreamcast's disc. */
		inline constexpr uint32 GdromTrack = MakeTag('C', 'H', 'G', 'D');
	}

	/** One metadata entry: a tagged piece of data that describes the image. */
	struct FMetadata
	{
		uint32 Tag = 0;
		uint8 Flags = 0;
		std::vector<uint8> Data;
	};

	/** Reads every metadata entry, in the order the file links them. */
	TResult<std::vector<FMetadata>> ReadMetadata(const IInput& Input, const FHeader& Header);
}
