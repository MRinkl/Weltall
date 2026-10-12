#pragma once

#include "Foundation/Types.h"

#include <initializer_list>
#include <vector>

namespace Weltall::Testing
{
	/** Builds little-endian test files. */
	class FByteWriter
	{
	public:
		FByteWriter& U8(uint8 Value)
		{
			Bytes.push_back(Value);
			return *this;
		}

		FByteWriter& U8s(std::initializer_list<uint8> Values)
		{
			Bytes.insert(Bytes.end(), Values);
			return *this;
		}

		FByteWriter& U16(uint16 Value)
		{
			return U8(static_cast<uint8>(Value)).U8(static_cast<uint8>(Value >> 8));
		}

		FByteWriter& S16(int16 Value) { return U16(static_cast<uint16>(Value)); }

		FByteWriter& U32(uint32 Value) { return U16(static_cast<uint16>(Value)).U16(static_cast<uint16>(Value >> 16)); }

		/** A VRAM rectangle as TIM blocks store it. */
		FByteWriter& Rect(int16 X, int16 Y, int16 Width, int16 Height) { return S16(X).S16(Y).S16(Width).S16(Height); }

		std::size_t Size() const { return Bytes.size(); }

		std::vector<uint8> Bytes;
	};
}
