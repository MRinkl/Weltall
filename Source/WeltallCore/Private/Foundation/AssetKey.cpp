#include "Foundation/AssetKey.h"

#include <cstdio>

namespace Weltall
{
	namespace
	{
		bool IsHexDigit(char Character)
		{
			return (Character >= '0' && Character <= '9') || (Character >= 'a' && Character <= 'f') || (Character >= 'A' && Character <= 'F');
		}
	}

	uint64 AssetKeyOfDigest(const FSha1::FDigest& Digest)
	{
		uint64 Key = 0;
		for (std::size_t Byte = 0; Byte < sizeof(Key); ++Byte)
		{
			Key = Key << 8 | Digest[Byte];
		}
		return Key;
	}

	uint64 AssetKeyOf(std::span<const uint8> Bytes)
	{
		return AssetKeyOfDigest(FSha1::Hash(Bytes));
	}

	std::string AssetKeyName(uint64 Key)
	{
		char Name[17];
		std::snprintf(Name, sizeof(Name), "%016llx", static_cast<unsigned long long>(Key));
		return Name;
	}

	std::optional<uint64> AssetKeyOfName(std::string_view Stem)
	{
		constexpr std::size_t Digits = 16;
		if (Stem.size() < Digits || (Stem.size() > Digits && IsHexDigit(Stem[Stem.size() - Digits - 1])))
		{
			return std::nullopt;
		}
		uint64 Key = 0;
		for (const char Character : Stem.substr(Stem.size() - Digits))
		{
			if (!IsHexDigit(Character))
			{
				return std::nullopt;
			}
			const int32 Digit = Character <= '9' ? Character - '0' : (Character | 0x20) - 'a' + 10;
			Key = Key << 4 | static_cast<uint64>(Digit);
		}
		return Key;
	}
}
