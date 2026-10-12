#include "Media/ChdDisc.h"

#include "Chd/ChdCdTracks.h"
#include "Chd/ChdImage.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>

namespace Weltall
{
	namespace
	{
		/** The core's bytes as the CHD reader's input. */
		class FChdInput final : public Chd::IInput
		{
		public:
			explicit FChdInput(std::shared_ptr<const IByteSource> InFile) : File(std::move(InFile)) {}

			uint64 Size() const override { return File->Size(); }
			bool Read(uint64 Offset, std::span<uint8> Destination) const override { return File->Read(Offset, Destination); }

		private:
			std::shared_ptr<const IByteSource> File;
		};

		/** A CHD's image of 2048-byte sectors (one chdman made of an ISO as a DVD) as the core's bytes. */
		class FChdBytes final : public IByteSource
		{
		public:
			explicit FChdBytes(std::unique_ptr<Chd::FImage> InImage) : Image(std::move(InImage)) {}

			uint64 Size() const override { return Image->Size(); }
			bool Read(uint64 Offset, std::span<uint8> Destination) const override { return Image->Read(Offset, Destination); }

		private:
			std::unique_ptr<Chd::FImage> Image;
		};

		/** A CD track's sectors as the core's bytes. */
		class FChdTrack final : public IByteSource
		{
		public:
			explicit FChdTrack(Chd::FCdTrackSectors InSectors) : Sectors(std::move(InSectors)) {}

			uint64 Size() const override { return Sectors.Size(); }
			bool Read(uint64 Offset, std::span<uint8> Destination) const override { return Sectors.Read(Offset, Destination); }

		private:
			Chd::FCdTrackSectors Sectors;
		};

		/** Why a CHD cannot be read, as the game library's note goes on after "Cannot be read: ". */
		FError ChdError(Chd::EError Error)
		{
			switch (Error)
			{
			case Chd::EError::CannotRead:
				return MakeError("the CHD cannot be read");
			case Chd::EError::UnsupportedVersion:
				return MakeError("a CHD of an old version (chdman's copy command makes a new one of it)");
			case Chd::EError::UnknownCodec:
				return MakeError("a CHD packed in a way the game cannot unpack");
			case Chd::EError::NeedsParent:
				return MakeError("a CHD that holds only its differences from another CHD, where the game needs a whole one");
			case Chd::EError::CutShort:
				return MakeError("the CHD ends too soon: still being copied, or a copy cut short");
			case Chd::EError::NotChd:
			case Chd::EError::Corrupt:
				break;
			}
			return MakeError("a damaged CHD: what it says of itself does not hold");
		}
	}

	bool IsChd(const IByteSource& Source)
	{
		constexpr std::array<char, 8> Signature{'M', 'C', 'o', 'm', 'p', 'r', 'H', 'D'};
		std::array<uint8, Signature.size()> Start{};
		return Source.Read(0, Start) && std::memcmp(Start.data(), Signature.data(), Signature.size()) == 0;
	}

	TResult<FCdImage> OpenChdImage(std::shared_ptr<const IByteSource> File)
	{
		auto Image = Chd::FImage::Open(std::make_shared<FChdInput>(std::move(File)));
		if (!Image)
		{
			return ChdError(Image.Error());
		}
		const uint32 UnitBytes = (*Image)->File().Header().UnitBytes;
		if (UnitBytes == CdSector::CookedSize)
		{
			return FCdImage(std::make_shared<FChdBytes>(std::move(*Image)), CdSector::CookedSize, 0);
		}
		if (UnitBytes != Chd::CdFrameBytes)
		{
			return MakeError("a CHD of something other than a CD");
		}
		const auto Tracks = Chd::ReadCdTracks((*Image)->File());
		if (!Tracks)
		{
			return ChdError(Tracks.Error());
		}
		const auto Data = std::find_if(Tracks->begin(), Tracks->end(), [](const Chd::FCdTrack& Track) { return !Track.IsAudio(); });
		if (Data == Tracks->end())
		{
			return MakeError("a CHD of a CD with no data track");
		}
		if (Data->SectorBytes != CdSector::RawSize && Data->SectorBytes != CdSector::CookedSize)
		{
			return MakeError("a CHD whose data track keeps " + std::to_string(Data->SectorBytes) + " bytes of each sector, where the game reads 2352 or 2048");
		}
		const uint32 SectorBytes = Data->SectorBytes;
		Chd::FCdTrackSectors Sectors(std::shared_ptr<const Chd::FImage>(std::move(*Image)), *Data);
		return FCdImage(std::make_shared<FChdTrack>(std::move(Sectors)), SectorBytes, 0);
	}
}
