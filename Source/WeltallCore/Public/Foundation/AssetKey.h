#pragma once

#include "Foundation/Sha1.h"
#include "Foundation/Types.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace Weltall
{
	/** A key naming a game asset by its content, wherever the game keeps it: the first 64 bits of a SHA-1. */
	WELTALLCORE_API uint64 AssetKeyOfDigest(const FSha1::FDigest& Digest);
	WELTALLCORE_API uint64 AssetKeyOf(std::span<const uint8> Bytes);

	/** A key as its files name it: 16 lowercase hexadecimal digits. */
	WELTALLCORE_API std::string AssetKeyName(uint64 Key);
	/** The key a file's name (without its extension) ends with, so that a name may describe the asset before it. */
	WELTALLCORE_API std::optional<uint64> AssetKeyOfName(std::string_view Stem);
}
