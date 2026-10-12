#pragma once

#include "Foundation/Types.h"

#include <array>
#include <span>
#include <string>

namespace Weltall
{
	/** SHA-1 (FIPS 180-4), used to recognize known game files, not for security. */
	class FSha1
	{
	public:
		using FDigest = std::array<uint8, 20>;

		WELTALLCORE_API FSha1();

		WELTALLCORE_API void Update(std::span<const uint8> Bytes);
		WELTALLCORE_API FDigest Finish();

		WELTALLCORE_API static FDigest Hash(std::span<const uint8> Bytes);
		WELTALLCORE_API static std::string ToHex(const FDigest& Digest);

	private:
		WELTALLCORE_API void ProcessBlock(const uint8* Block);

		std::array<uint32, 5> State;
		std::array<uint8, 64> Buffer{};
		std::size_t Buffered = 0;
		uint64 TotalBytes = 0;
	};
}
