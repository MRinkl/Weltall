#pragma once

#include "Foundation/ByteSource.h"
#include "Foundation/Result.h"

#include <cstdio>
#include <filesystem>
#include <memory>
#include <mutex>

namespace Weltall
{
	/** A file read through the C standard library, safe to read from several threads at once. */
	class FFileByteSource final : public IByteSource
	{
	public:
		WELTALLCORE_API static TResult<std::unique_ptr<FFileByteSource>> Open(const std::filesystem::path& Path);

		WELTALLCORE_API ~FFileByteSource() override;
		FFileByteSource(const FFileByteSource&) = delete;
		FFileByteSource& operator=(const FFileByteSource&) = delete;

		uint64 Size() const override { return FileSize; }
		WELTALLCORE_API bool Read(uint64 Offset, std::span<uint8> Destination) const override;

	private:
		FFileByteSource(std::FILE* InFile, uint64 InSize) : File(InFile), FileSize(InSize) {}

		std::FILE* File;
		uint64 FileSize;
		/** A read is a seek then a read of the one position the file keeps. */
		mutable std::mutex Guard;
	};
}
