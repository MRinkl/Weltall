#include "Chd/ChdImage.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <thread>

namespace Chd
{
	namespace
	{
		/**
		 * A read uses a thread more for every so many bytes of whole hunks, up to a few: unpacking is the slow part of
		 * reading. Sixteen of a DVD's 4 KiB hunks, three or four of a CD's eight frames.
		 */
		constexpr uint64 BytesPerThread = 64 * 1024;
		constexpr unsigned MostThreads = 8;
	}

	TResult<std::unique_ptr<FImage>> FImage::Open(std::shared_ptr<const IInput> Input)
	{
		auto File = FFile::Open(std::move(Input));
		if (!File)
		{
			return File.Error();
		}
		return std::make_unique<FImage>(std::move(*File));
	}

	FImage::FImage(FFile InFile) : ChdFile(std::move(InFile))
	{
	}

	bool FImage::Read(uint64 Offset, std::span<uint8> Destination) const
	{
		if (Offset > Size() || Destination.size() > Size() - Offset)
		{
			return false;
		}
		const std::lock_guard<std::mutex> Lock(Guard);
		const uint32 HunkBytes = ChdFile.Header().HunkBytes;
		for (std::size_t Done = 0; Done < Destination.size();)
		{
			const uint64 At = Offset + Done;
			const uint32 Hunk = static_cast<uint32>(At / HunkBytes);
			const uint32 Within = static_cast<uint32>(At % HunkBytes);
			const std::size_t Whole = Within == 0 ? (Destination.size() - Done) / HunkBytes : 0;
			if (Whole != 0)
			{
				if (!ReadWholeHunks(Hunk, static_cast<uint32>(Whole), Destination.subspan(Done, Whole * HunkBytes)))
				{
					return false;
				}
				Done += Whole * HunkBytes;
				continue;
			}
			const std::size_t Take = std::min<std::size_t>(HunkBytes - Within, Destination.size() - Done);
			const std::vector<uint8>* Bytes = CachedHunk(Hunk);
			if (Bytes == nullptr)
			{
				return false;
			}
			std::memcpy(Destination.data() + Done, Bytes->data() + Within, Take);
			Done += Take;
		}
		return true;
	}

	bool FImage::ReadWholeHunks(uint32 First, uint32 Count, std::span<uint8> Destination) const
	{
		const uint32 HunkBytes = ChdFile.Header().HunkBytes;
		const auto ReadOne = [this, First, HunkBytes, Destination](uint32 Index, FHunkDecoder& HunkDecoder) {
			const std::span<uint8> Bytes = Destination.subspan(static_cast<std::size_t>(Index) * HunkBytes, HunkBytes);
			const uint32 Hunk = First + Index;
			const auto Cached = std::find_if(Cache.begin(), Cache.end(), [Hunk](const FCachedHunk& Slot) { return Slot.Hunk == Hunk; });
			if (Cached != Cache.end())
			{
				std::memcpy(Bytes.data(), Cached->Bytes.data(), HunkBytes);
				return true;
			}
			return ChdFile.ReadHunk(Hunk, Bytes, HunkDecoder);
		};

		const uint64 ThreadsForBytes = static_cast<uint64>(Count) * HunkBytes / BytesPerThread;
		const unsigned Threads = static_cast<unsigned>(std::min<uint64>({std::max(1u, std::thread::hardware_concurrency()), MostThreads, ThreadsForBytes}));
		if (Threads <= 1)
		{
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				if (!ReadOne(Index, Decoder))
				{
					return false;
				}
			}
			return true;
		}
		while (Helpers.size() < Threads - 1)
		{
			Helpers.push_back(std::make_unique<FHunkDecoder>());
		}
		// The threads take the hunks in turns from a shared count; the cache is only read until they are done.
		std::atomic<uint32> Next{0};
		std::atomic<bool> bFailed{false};
		const auto Work = [&](FHunkDecoder* HunkDecoder) {
			for (uint32 Index = Next++; Index < Count && !bFailed; Index = Next++)
			{
				if (!ReadOne(Index, *HunkDecoder))
				{
					bFailed = true;
				}
			}
		};
		std::vector<std::thread> Helping;
		for (unsigned Helper = 0; Helper + 1 < Threads; ++Helper)
		{
			Helping.emplace_back(Work, Helpers[Helper].get());
		}
		Work(&Decoder);
		for (std::thread& Thread : Helping)
		{
			Thread.join();
		}
		return !bFailed;
	}

	const std::vector<uint8>* FImage::CachedHunk(uint32 Hunk) const
	{
		const auto Found = std::find_if(Cache.begin(), Cache.end(), [Hunk](const FCachedHunk& Slot) { return Slot.Hunk == Hunk; });
		FCachedHunk& Slot = Found != Cache.end()
			? *Found
			: *std::min_element(Cache.begin(), Cache.end(), [](const FCachedHunk& A, const FCachedHunk& B) { return A.LastUse < B.LastUse; });
		Slot.LastUse = ++Uses;
		if (Slot.Hunk == Hunk)
		{
			return &Slot.Bytes;
		}
		Slot.Bytes.resize(ChdFile.Header().HunkBytes);
		Slot.Hunk = ChdFile.ReadHunk(Hunk, Slot.Bytes, Decoder) ? Hunk : NoHunk;
		return Slot.Hunk == Hunk ? &Slot.Bytes : nullptr;
	}
}
