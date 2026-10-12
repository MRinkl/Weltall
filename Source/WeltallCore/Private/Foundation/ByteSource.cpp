#include "Foundation/ByteSource.h"

#include <algorithm>

namespace Weltall
{
	bool FMemoryByteSource::Read(uint64 Offset, std::span<uint8> Destination) const
	{
		if (Offset > Bytes.size() || Destination.size() > Bytes.size() - Offset)
		{
			return false;
		}
		std::copy_n(Bytes.begin() + static_cast<std::ptrdiff_t>(Offset), Destination.size(), Destination.begin());
		return true;
	}
}
