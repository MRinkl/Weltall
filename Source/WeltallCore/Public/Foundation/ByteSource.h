#pragma once

#include "Foundation/Types.h"

#include <span>
#include <vector>

namespace Weltall
{
	/** Random-access, read-only bytes: a file, a region of one, or memory. */
	class IByteSource
	{
	public:
		virtual ~IByteSource() = default;

		virtual uint64 Size() const = 0;

		/** Fills Destination with the bytes at Offset; false if they are not all available. */
		virtual bool Read(uint64 Offset, std::span<uint8> Destination) const = 0;
	};

	/** Bytes held in memory, for tests and for data that is already loaded. */
	class FMemoryByteSource final : public IByteSource
	{
	public:
		explicit FMemoryByteSource(std::vector<uint8> InBytes) : Bytes(std::move(InBytes)) {}

		uint64 Size() const override { return Bytes.size(); }
		WELTALLCORE_API bool Read(uint64 Offset, std::span<uint8> Destination) const override;

	private:
		std::vector<uint8> Bytes;
	};
}
