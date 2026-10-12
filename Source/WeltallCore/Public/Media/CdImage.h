#pragma once

#include "Foundation/ByteSource.h"
#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <array>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace Weltall
{
	namespace CdSector
	{
		constexpr uint32 RawSize = 2352;
		constexpr uint32 CookedSize = 2048;
		constexpr uint32 Form1DataSize = 2048;
		constexpr uint32 Form2DataSize = 2324;
		/** Submode bits of a Mode 2 subheader. */
		constexpr uint8 SubmodeVideo = 0x02;
		constexpr uint8 SubmodeAudio = 0x04;
		constexpr uint8 SubmodeData = 0x08;
		constexpr uint8 SubmodeForm2 = 0x20;
	}

	/** Mode and CD-ROM XA subheader of one sector. */
	struct FCdSectorHeader
	{
		uint8 Mode = 1;
		uint8 FileNumber = 0;
		uint8 Channel = 0;
		uint8 Submode = CdSector::SubmodeData;
		/** How an XA-ADPCM sector's sound is coded (FXaCodingInfo). */
		uint8 CodingInfo = 0;

		bool IsForm2() const { return Mode == 2 && (Submode & CdSector::SubmodeForm2) != 0; }
	};

	/**
	 * The data track of a CD image: raw 2352-byte sectors (every Mode 2 form is readable) or cooked
	 * 2048-byte ones (Form 1 data only; Form 2 sectors such as XA audio are simply absent).
	 */
	class FCdImage
	{
	public:
		/** Opens a .cue sheet (its first data track), a CHD (the same) or a single .bin/.iso/.img file. */
		WELTALLCORE_API static TResult<FCdImage> Open(const std::filesystem::path& Path);

		/** A data track of SectorSize-byte sectors starting TrackOffset bytes into Source. */
		WELTALLCORE_API FCdImage(std::shared_ptr<const IByteSource> InSource, uint32 InSectorSize, uint64 InTrackOffset);

		bool IsRaw() const { return SectorSize == CdSector::RawSize; }
		uint32 SectorCount() const { return Sectors; }

		WELTALLCORE_API TResult<FCdSectorHeader> ReadHeader(uint32 Lba) const;

		/** The sector's user data: 2048 bytes for Mode 1 and Form 1, 2324 for Form 2. */
		WELTALLCORE_API TResult<std::vector<uint8>> ReadUserData(uint32 Lba) const;

		/** Size bytes of Form 1 user data from consecutive sectors, starting at Lba. */
		WELTALLCORE_API TResult<std::vector<uint8>> ReadForm1(uint32 Lba, uint64 Size) const;

	private:
		WELTALLCORE_API bool ReadRaw(uint32 Lba, std::span<uint8> Destination) const;

		std::shared_ptr<const IByteSource> Source;
		uint32 SectorSize;
		uint64 TrackOffset;
		uint32 Sectors;
	};
}
