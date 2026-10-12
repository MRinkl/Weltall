#include "Sound/SoundRam.h"

namespace Weltall
{
	namespace
	{
		constexpr uint32 FixedBlockBytes = 0x1010;
	}

	FSoundRam::FSoundRam()
	{
		Blocks[0] = {true, 0, 0, FixedBlockBytes};
	}

	uint32 FSoundRam::Insert(int32 After, uint32 Address, uint32 Bytes)
	{
		for (int32 Index = 0; Index < BlockCount; ++Index)
		{
			FBlock& Block = Blocks[static_cast<std::size_t>(Index)];
			if (!Block.bUsed)
			{
				FBlock& Previous = Blocks[static_cast<std::size_t>(After)];
				Block = {true, Previous.Next, Address, Bytes};
				Previous.Next = Index;
				return Address;
			}
		}
		// The original then overwrites its first block; the port refuses instead.
		return 0;
	}

	uint32 FSoundRam::Reserve(uint32 Bytes)
	{
		int32 Previous = 0;
		uint32 End = Blocks[0].Address + Blocks[0].Bytes;
		while (Blocks[static_cast<std::size_t>(Previous)].Next != 0)
		{
			const FBlock& Next = Blocks[static_cast<std::size_t>(Blocks[static_cast<std::size_t>(Previous)].Next)];
			if (static_cast<int32>(Bytes) <= static_cast<int32>(Next.Address - End))
			{
				return Insert(Previous, End, Bytes);
			}
			End = Next.Address + Next.Bytes;
			Previous = Blocks[static_cast<std::size_t>(Previous)].Next;
		}
		if (static_cast<int32>(Bytes) > static_cast<int32>(Size - End))
		{
			return 0;
		}
		return Insert(Previous, End, Bytes);
	}

	uint32 FSoundRam::ReserveAt(uint32 Bytes, uint32 Address)
	{
		// Finds the gap before the first block that ends past the request. The size is compared with
		// the whole gap, not with the room after Address, so a request at the end of sound RAM can
		// run past it: the driver's own check, kept.
		int32 Previous = 0;
		uint32 PreviousEnd = Blocks[0].Address + Blocks[0].Bytes;
		uint32 Start = Blocks[0].Address;
		int32 Gap = 0;
		while (static_cast<int32>(Address) > static_cast<int32>(Start))
		{
			const int32 Next = Blocks[static_cast<std::size_t>(Previous)].Next;
			if (Next == 0)
			{
				Gap = static_cast<int32>(Size - PreviousEnd);
				break;
			}
			Start = Blocks[static_cast<std::size_t>(Next)].Address;
			if (static_cast<int32>(Address + Bytes) <= static_cast<int32>(Start))
			{
				Gap = static_cast<int32>(Start - PreviousEnd);
				break;
			}
			PreviousEnd = Start + Blocks[static_cast<std::size_t>(Next)].Bytes;
			Previous = Next;
		}
		if (static_cast<int32>(Bytes) > Gap || static_cast<int32>(PreviousEnd) > static_cast<int32>(Address))
		{
			return 0;
		}
		return Insert(Previous, Address, Bytes);
	}

	uint32 FSoundRam::Free(uint32 Address)
	{
		// The fixed first block is never freed (the original would unlink it through a null pointer).
		int32 Previous = 0;
		int32 Current = Blocks[0].Next;
		while (Current != 0)
		{
			FBlock& Block = Blocks[static_cast<std::size_t>(Current)];
			if (Block.Address == Address)
			{
				Blocks[static_cast<std::size_t>(Previous)].Next = Block.Next;
				Block.bUsed = false;
				Block.Next = 0;
				Block.Address = 0;
				return Address;
			}
			Previous = Current;
			Current = Block.Next;
		}
		return 0;
	}
}
