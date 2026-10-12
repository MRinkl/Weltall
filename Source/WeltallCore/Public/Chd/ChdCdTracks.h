#pragma once

#include "Chd/ChdCdSector.h"
#include "Chd/ChdFile.h"
#include "Chd/ChdImage.h"
#include "Chd/ChdTypes.h"

#include <memory>
#include <vector>

namespace Chd
{
	/** How a CD track's sectors are kept, as chdman names it from the CUE sheet. */
	enum class ECdTrackType : uint8
	{
		/** Mode 1 sectors' 2048 bytes of data. */
		Mode1,
		Mode1Raw,
		/** Mode 2 sectors' 2336 bytes after the header. */
		Mode2,
		/** Mode 2 Form 1 sectors' 2048 bytes of data. */
		Mode2Form1,
		/** Mode 2 Form 2 sectors' 2324 bytes of data. */
		Mode2Form2,
		/** Mode 2 sectors of either form, 2336 bytes each. */
		Mode2FormMix,
		Mode2Raw,
		/** CD sound: 588 stereo samples of 16 bits a sector. */
		Audio,
	};

	/** One track of a CD, as its metadata describes it and where its frames lie in the CHD. */
	struct FCdTrack
	{
		uint32 Number = 0;
		ECdTrackType Type = ECdTrackType::Mode1;
		/** How many bytes each frame keeps of its sector: 2352 for a raw sector or sound, fewer for cooked data. */
		uint32 SectorBytes = 0;
		/** The frames the image holds of the track, a pregap it keeps first among them. */
		uint32 Frames = 0;
		uint32 Pregap = 0;
		/** Whether the image keeps the pregap's sectors; when it does not, they are not in the image at all. */
		bool bPregapInImage = false;
		/** The track's first frame among the CHD's units: tracks follow each other, each padded to four frames. */
		uint64 FirstFrame = 0;

		bool IsAudio() const { return Type == ECdTrackType::Audio; }
		/** The sectors of the track itself, after a pregap the image keeps. */
		uint32 SectorCount() const { return Frames - KeptPregap(); }
		/** The frame of the track's first own sector. */
		uint64 FirstSectorFrame() const { return FirstFrame + KeptPregap(); }

	private:
		uint32 KeptPregap() const { return bPregapInImage ? Pregap : 0; }
	};

	/**
	 * A CD's tracks from a CHD's metadata, in their order; none when the CHD holds no CD. Corrupt when the
	 * metadata cannot be read or places a track beyond the image.
	 */
	TResult<std::vector<FCdTrack>> ReadCdTracks(const FFile& File);

	/**
	 * A CD track's own sectors one after another, each its SectorBytes as a raw image (BIN) keeps them: a CHD keeps
	 * sound big-endian, a BIN little-endian.
	 */
	class FCdTrackSectors
	{
	public:
		FCdTrackSectors(std::shared_ptr<const FImage> InImage, FCdTrack InTrack);

		const FCdTrack& Track() const { return CdTrack; }
		uint64 Size() const { return static_cast<uint64>(CdTrack.SectorCount()) * CdTrack.SectorBytes; }

		/** Fills Destination with the bytes at Offset; false when the track ends before them or they cannot be read. */
		bool Read(uint64 Offset, std::span<uint8> Destination) const;

	private:
		std::shared_ptr<const FImage> Image;
		FCdTrack CdTrack;
	};
}
