#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	/**
	 * Section Number (1-based) of a section container, unpacked: the file is a count of sections, then
	 * each section's offset, and each section is an LZSS stream. As the game does, a stream may read
	 * on into the next section.
	 */
	WELTALLCORE_API TResult<std::vector<uint8>> UnpackContainerSection(std::span<const uint8> File, uint32 Number);

	/**
	 * How many sections a file of unknown kind holds if it has a container's shape: a count from 1 to 64, then as many
	 * offsets, rising, past them and within the file. None otherwise.
	 */
	WELTALLCORE_API std::optional<uint32> ContainerSectionCount(std::span<const uint8> File);
}
