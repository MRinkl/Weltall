#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Game/OriginalSave.h"

#include <span>
#include <vector>

namespace Weltall
{
	class FMenuResources;
	class FSaveStore;

	/**
	 * One of the player's save stores (from 0) as a raw memory card image, for an emulator or a console: each of
	 * its saves a file as the game's Save writes one, under the game's name for its slot and with its header,
	 * in the slots' order. An error when one of its saves cannot be read or is damaged.
	 */
	WELTALLCORE_API TResult<std::vector<uint8>> ExportStoreCard(const FSaveStore& Saves, int32 Store, const FMenuResources& Resources);

	/**
	 * The game's saves in a memory card image or a single save's file (ReadMemoryCardImage's kinds), each as the
	 * Menu would load it and in the file's order; other games' files and saves whose checksum fails are left out.
	 * An error when the file is no such image.
	 */
	WELTALLCORE_API TResult<std::vector<FOriginalSave>> ReadCardImageSaves(std::span<const uint8> File);
}
