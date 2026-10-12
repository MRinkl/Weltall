#pragma once

#include "Chd/ChdFile.h"
#include "Chd/ChdHunkDecoder.h"
#include "Chd/ChdTypes.h"

#include <array>
#include <memory>
#include <mutex>
#include <vector>

namespace Chd
{
	/**
	 * A CHD's image as the bytes it packs: its units one after another, unpacked a hunk at a time as they are read.
	 * The last few hunks read in part are kept, so that reading on in small pieces unpacks each hunk once; a long read
	 * unpacks its hunks on several threads.
	 */
	class FImage
	{
	public:
		static TResult<std::unique_ptr<FImage>> Open(std::shared_ptr<const IInput> Input);

		explicit FImage(FFile InFile);
		FImage(const FImage&) = delete;
		FImage& operator=(const FImage&) = delete;

		const FFile& File() const { return ChdFile; }

		/** The image's size: the header's logical bytes. */
		uint64 Size() const { return ChdFile.Header().LogicalBytes; }

		/**
		 * Fills Destination with the image's bytes at Offset; false when the image ends before them or a hunk they lie
		 * in cannot be unpacked. Safe to call from several threads.
		 */
		bool Read(uint64 Offset, std::span<uint8> Destination) const;

	private:
		static constexpr uint32 NoHunk = 0xFFFFFFFF;

		struct FCachedHunk
		{
			uint32 Hunk = NoHunk;
			uint64 LastUse = 0;
			std::vector<uint8> Bytes;
		};

		/** The hunk's bytes, from the cache or unpacked into the slot used longest ago; null when it cannot be unpacked. */
		const std::vector<uint8>* CachedHunk(uint32 Hunk) const;

		/** Unpacks Count whole hunks from First on straight into Destination, on several threads when there are enough. */
		bool ReadWholeHunks(uint32 First, uint32 Count, std::span<uint8> Destination) const;

		FFile ChdFile;
		mutable std::mutex Guard;
		mutable FHunkDecoder Decoder;
		mutable std::array<FCachedHunk, 4> Cache;
		mutable uint64 Uses = 0;
		/** The codecs' state of the threads that help with long reads, made when first needed. */
		mutable std::vector<std::unique_ptr<FHunkDecoder>> Helpers;
	};
}
