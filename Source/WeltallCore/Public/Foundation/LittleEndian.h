#pragma once

#include "Foundation/Types.h"

#include <span>

namespace Weltall::LittleEndian
{
	/** Reads little-endian integers from a byte span; the caller guarantees the bytes exist. */
	inline uint16 ReadU16(std::span<const uint8> Bytes, std::size_t Offset)
	{
		return static_cast<uint16>(Bytes[Offset] | Bytes[Offset + 1] << 8);
	}

	inline uint32 ReadU32(std::span<const uint8> Bytes, std::size_t Offset)
	{
		return static_cast<uint32>(Bytes[Offset]) | static_cast<uint32>(Bytes[Offset + 1]) << 8
			| static_cast<uint32>(Bytes[Offset + 2]) << 16 | static_cast<uint32>(Bytes[Offset + 3]) << 24;
	}

	inline int16 ReadS16(std::span<const uint8> Bytes, std::size_t Offset)
	{
		return static_cast<int16>(ReadU16(Bytes, Offset));
	}

	/** Writes little-endian integers into a byte span; the caller guarantees the bytes exist. */
	inline void WriteU16(std::span<uint8> Bytes, std::size_t Offset, uint16 Value)
	{
		Bytes[Offset] = static_cast<uint8>(Value);
		Bytes[Offset + 1] = static_cast<uint8>(Value >> 8);
	}

	inline void WriteU32(std::span<uint8> Bytes, std::size_t Offset, uint32 Value)
	{
		WriteU16(Bytes, Offset, static_cast<uint16>(Value));
		WriteU16(Bytes, Offset + 2, static_cast<uint16>(Value >> 16));
	}
}
