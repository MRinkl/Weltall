#pragma once

#include "Chd/ChdTypes.h"

namespace Chd
{
	/** A CD sector as a raw image keeps it, sync and error-correction bytes included. */
	inline constexpr uint32 CdSectorBytes = 2352;
	/** A CD's unit in a CHD: a raw sector, then 96 bytes of subchannel data. */
	inline constexpr uint32 CdFrameBytes = 2448;
	inline constexpr uint32 CdSubchannelBytes = CdFrameBytes - CdSectorBytes;

	/**
	 * Writes a raw sector's sync pattern and computes its error-correction bytes (ECMA-130's P and Q parity) from its
	 * header and data: a CHD's CD codecs leave both out wherever they can be made again. In a Mode 2 sector the header
	 * counts as zeros.
	 */
	void RestoreSyncAndEcc(std::span<uint8, CdSectorBytes> Sector);
}
