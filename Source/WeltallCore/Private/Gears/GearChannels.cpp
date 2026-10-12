#include "Gears/GearChannels.h"

namespace Weltall
{
	namespace
	{
		constexpr std::size_t MaxChannels = 0xFFFF;
	}

	void FGearChannelPool::Init(int32 Capacity, bool bGrowsWhenFull)
	{
		Size = static_cast<uint16>(Capacity);
		Entries.assign(static_cast<std::size_t>(Size), FGearChannel{});
		Lowest = 0;
		bGrows = bGrowsWhenFull;
	}

	void FGearChannelPool::Release()
	{
		Entries.clear();
		Entries.shrink_to_fit();
		Lowest = 0;
	}

	int32 FGearChannelPool::Allocate()
	{
		if (Lowest >= Size)
		{
			if (!bGrows || Entries.size() >= MaxChannels || Entries.empty())
			{
				return None;
			}
			Entries.emplace_back();
			Size = static_cast<uint16>(Entries.size());
			Lowest = Size;
			return Size - 1;
		}
		if (Entries.empty() || Entries[Lowest].bUsed)
		{
			return None;
		}
		const int32 Found = Lowest;
		++Lowest;
		while (Lowest < Size && Entries[Lowest].bUsed)
		{
			++Lowest;
		}
		return Found;
	}

	void FGearChannelPool::Free(int32 Index)
	{
		if (Index < 0 || static_cast<std::size_t>(Index) >= Entries.size())
		{
			return;
		}
		if (Index < Lowest)
		{
			Lowest = static_cast<uint16>(Index);
		}
		Entries[static_cast<std::size_t>(Index)].bUsed = false;
	}
}
