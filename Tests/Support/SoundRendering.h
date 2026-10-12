#pragma once

#include "Sound/GameSound.h"

#include <string>
#include <vector>

namespace Weltall::Testing
{
	/** Makes Frames samples of a sound in buffers of BufferFrames, as an audio device asks for them. */
	std::vector<int16> RenderSound(IGameSound& Sound, int32 Frames, int32 BufferFrames = 1024);

	/** The loudest sample. */
	int32 Peak(const std::vector<int16>& Samples);

	/** The SHA-1 of samples as little-endian 16-bit values, in hex. */
	std::string DigestSamples(const std::vector<int16>& Samples);
}
