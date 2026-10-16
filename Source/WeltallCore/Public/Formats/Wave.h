#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	struct FWaveLoop
	{
		uint32 FirstFrame = 0;
		/** The first frame after the loop. */
		uint32 EndFrame = 0;
	};

	/** Integer PCM WAVE, with an optional forward sampler loop. Samples are interleaved. */
	struct FWave
	{
		uint32 SampleRate = 44100;
		uint16 Channels = 2;
		std::vector<int16> Samples;
		std::optional<FWaveLoop> Loop;

		std::size_t Frames() const { return Channels == 0 ? 0 : Samples.size() / Channels; }
		WELTALLCORE_API static TResult<FWave> Parse(std::span<const uint8> Bytes);
		WELTALLCORE_API TResult<std::vector<uint8>> Serialize() const;
	};
}
