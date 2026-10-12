#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Media/CdImage.h"

#include <string>
#include <string_view>
#include <vector>

namespace Weltall
{
	/** CD-ROM XA attribute bits of a directory record. */
	namespace XaAttribute
	{
		constexpr uint16 Form1 = 0x0800;
		constexpr uint16 Form2 = 0x1000;
		constexpr uint16 Interleaved = 0x2000;
		constexpr uint16 CdAudio = 0x4000;
		constexpr uint16 Directory = 0x8000;
	}

	struct FIsoEntry
	{
		/** Path from the root with '/' separators and without the ";1" version, e.g. "SLUS_006.64". */
		std::string Path;
		uint32 Lba = 0;
		uint32 Size = 0;
		bool bDirectory = false;
		uint16 XaAttributes = 0;

		uint32 SectorCount() const { return (Size + CdSector::Form1DataSize - 1) / CdSector::Form1DataSize; }
		/** Interleaved (movie) and Form 2 files cannot be read as plain Form 1 data. */
		bool IsForm1Data() const { return (XaAttributes & (XaAttribute::Form2 | XaAttribute::Interleaved)) == 0; }
	};

	/** The ISO 9660 file system of a PlayStation disc. */
	class FIso9660
	{
	public:
		/** Reads the volume descriptor and the whole directory tree. */
		WELTALLCORE_API static TResult<FIso9660> Open(FCdImage Image);

		const std::vector<FIsoEntry>& Entries() const { return AllEntries; }

		/** Case-insensitive lookup; accepts '\' or '/' separators, a leading separator and ";1". */
		WELTALLCORE_API const FIsoEntry* Find(std::string_view Path) const;

		WELTALLCORE_API TResult<std::vector<uint8>> ReadFile(const FIsoEntry& Entry) const;
		WELTALLCORE_API TResult<std::vector<uint8>> ReadFile(std::string_view Path) const;
		/** Size bytes starting Offset bytes into a Form 1 file. */
		WELTALLCORE_API TResult<std::vector<uint8>> ReadFileRange(const FIsoEntry& Entry, uint64 Offset, uint64 Size) const;

		const FCdImage& Image() const { return CdImage; }

	private:
		WELTALLCORE_API FIso9660(FCdImage InImage, std::vector<FIsoEntry> InEntries);

		FCdImage CdImage;
		std::vector<FIsoEntry> AllEntries;
	};
}
