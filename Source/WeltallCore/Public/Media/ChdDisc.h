#pragma once

#include "Foundation/ByteSource.h"
#include "Foundation/Result.h"
#include "Media/CdImage.h"

#include <memory>

namespace Weltall
{
	/** Whether the bytes begin as a CHD does. */
	WELTALLCORE_API bool IsChd(const IByteSource& Source);

	/**
	 * The CD a CHD packs, as the game reads it: the sectors of its first data track as a BIN or an ISO keeps them,
	 * unpacked as they are read. File must allow reads from several threads at once.
	 */
	WELTALLCORE_API TResult<FCdImage> OpenChdImage(std::shared_ptr<const IByteSource> File);
}
