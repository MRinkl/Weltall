#pragma once

#include "Chd/ChdTypes.h"

#include <bit>
#include <cstring>

/** Big-endian integers, as a CHD stores its header, map and metadata; the caller guarantees the bytes exist. */
namespace Chd::BigEndian
{
	namespace Detail
	{
		/** Each value is loaded whole and then put in order: a compiler has been seen to miscompile one assembled byte by byte. */
		template <typename T>
		T Load(std::span<const uint8> Bytes, std::size_t Offset)
		{
			T Value;
			std::memcpy(&Value, Bytes.data() + Offset, sizeof(Value));
			if constexpr (std::endian::native == std::endian::little)
			{
				T Swapped = 0;
				for (std::size_t Index = 0; Index < sizeof(T); ++Index)
				{
					Swapped = static_cast<T>(Swapped << 8 | (Value & 0xFF));
					Value = static_cast<T>(Value >> 8);
				}
				Value = Swapped;
			}
			return Value;
		}
	}

	inline uint16 ReadU16(std::span<const uint8> Bytes, std::size_t Offset)
	{
		return Detail::Load<uint16>(Bytes, Offset);
	}

	inline uint32 ReadU24(std::span<const uint8> Bytes, std::size_t Offset)
	{
		return static_cast<uint32>(Detail::Load<uint16>(Bytes, Offset)) << 8 | Bytes[Offset + 2];
	}

	inline uint32 ReadU32(std::span<const uint8> Bytes, std::size_t Offset)
	{
		return Detail::Load<uint32>(Bytes, Offset);
	}

	inline uint64 ReadU48(std::span<const uint8> Bytes, std::size_t Offset)
	{
		return static_cast<uint64>(Detail::Load<uint16>(Bytes, Offset)) << 32 | Detail::Load<uint32>(Bytes, Offset + 2);
	}

	inline uint64 ReadU64(std::span<const uint8> Bytes, std::size_t Offset)
	{
		return Detail::Load<uint64>(Bytes, Offset);
	}
}
