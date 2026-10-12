#pragma once

#include "Chd/ChdTypes.h"

namespace Chd
{
	/**
	 * The CRC-16 a CHD keeps of its map and of each hunk: CCITT's polynomial 0x1021, from 0xFFFF, unreflected.
	 * Given the CRC of the bytes before, it goes on from there.
	 */
	uint16 Crc16(std::span<const uint8> Bytes, uint16 Crc = 0xFFFF);
}
