#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <filesystem>
#include <span>

namespace Weltall
{
	/**
	 * Saves Bytes as Root / Relative unless a file already has that name: true when saved, false when kept. The bytes
	 * are written beside it first and published whole; a path leaving Root or passing a link is refused.
	 */
	WELTALLCORE_API TResult<bool> PublishAssetFile(const std::filesystem::path& Root, const std::filesystem::path& Relative, std::span<const uint8> Bytes);
}
