#pragma once

#include "Foundation/Types.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	/**
	 * Brings 37800 Hz XA-ADPCM sound to 44100 Hz as the drive's decoder does: after every six input
	 * samples it outputs seven, each a 29-point filter over the latest input.
	 */
	class FXaResampler
	{
	public:
		/**
		 * Resamples frames of Channels (1 or 2) interleaved samples and appends the result to Out,
		 * always as stereo frames (mono is sent to both sides).
		 */
		WELTALLCORE_API void Resample(std::span<const int16> Input, int32 Channels, std::vector<int16>& Out);

	private:
		static constexpr int32 RingSize = 32;

		std::array<std::array<int16, RingSize>, 2> Ring{};
		int32 Position = 0;
		int32 UntilOutput = 6;
	};
}
