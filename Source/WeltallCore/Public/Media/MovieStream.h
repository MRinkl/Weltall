#pragma once

#include "Formats/StrVideo.h"
#include "Formats/XaAdpcm.h"
#include "Foundation/Result.h"
#include "Media/CdImage.h"
#include "Sound/XaResampler.h"

#include <optional>
#include <vector>

namespace Weltall
{
	/**
	 * A movie read in disc order, as the drive streams it: its video sectors put together into
	 * compressed frames, and its XA-ADPCM sound decoded and brought to 44100 Hz stereo.
	 */
	class FMovieStream
	{
	public:
		/** The movie's sectors, from its first; the image must outlive the stream. */
		WELTALLCORE_API FMovieStream(const FCdImage& InImage, uint32 FirstLba, uint32 SectorCount);

		/**
		 * Reads on to the next complete video frame, appending the sound met on the way to Sound
		 * (interleaved left and right). Nothing once the movie has ended.
		 */
		WELTALLCORE_API TResult<std::optional<FStrFrame>> NextFrame(std::vector<int16>& Sound);

	private:
		const FCdImage& Image;
		uint32 NextLba;
		uint32 EndLba;
		FStrFrameAssembler Frames;
		std::array<FXaChannelHistory, 2> History{};
		FXaResampler Resampler;
		std::vector<int16> Decoded;
	};
}
