#pragma once

#include "Foundation/Types.h"

#include <array>

namespace Weltall
{
	/**
	 * The driver's map of sound RAM: up to twelve reserved blocks in a list sorted by address, the
	 * first always covering the SPU's capture buffers and the silent block (0 to 100Fh). Addresses
	 * are bytes; a failed reservation returns 0.
	 */
	class FSoundRam
	{
	public:
		static constexpr uint32 Size = 0x80000;

		FSoundRam();

		/** The first gap large enough, from the bottom. */
		uint32 Reserve(uint32 Bytes);
		/** Bytes at Address, if that lies in a gap (the check the driver makes, not a strict one: see the source). */
		uint32 ReserveAt(uint32 Bytes, uint32 Address);
		/** Frees the block at Address; returns Address, or 0 when no block starts there. */
		uint32 Free(uint32 Address);

	private:
		struct FBlock
		{
			bool bUsed = false;
			int32 Next = 0;
			uint32 Address = 0;
			uint32 Bytes = 0;
		};

		static constexpr int32 BlockCount = 12;

		/** Adds a block after After; 0 when the table is full. */
		uint32 Insert(int32 After, uint32 Address, uint32 Bytes);

		std::array<FBlock, BlockCount> Blocks{};
	};
}
