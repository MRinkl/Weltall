#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <span>
#include <string>
#include <vector>

namespace Weltall
{
	/** One save on a memory card: the file name its directory entry gives it, and its blocks in order. */
	struct FCardSave
	{
		std::string FileName;
		std::vector<uint8> Blocks;
	};

	namespace MemoryCard
	{
		inline constexpr std::size_t CardSize = 0x20000;
		inline constexpr std::size_t BlockSize = 0x2000;
		inline constexpr std::size_t FrameSize = 0x80;
	}

	/**
	 * The saves in a memory-card image file: a whole card (raw, or behind a DexDrive, Virtual Game
	 * Station or PSV header, possibly cut short of its unused blocks) or one save on its own (MCS,
	 * Xploder or Action Replay). A file that is none of these is an error.
	 */
	WELTALLCORE_API TResult<std::vector<FCardSave>> ReadMemoryCardImage(std::span<const uint8> File);

	/**
	 * A raw image of a formatted memory card holding Saves, each in the next free blocks in the order given.
	 * An error when a save's blocks are not whole, its name is longer than the directory's, or the card's
	 * 15 blocks cannot hold them all.
	 */
	WELTALLCORE_API TResult<std::vector<uint8>> WriteMemoryCardImage(std::span<const FCardSave> Saves);
}
