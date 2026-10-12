#pragma once

#include "Foundation/Types.h"

#include <span>

namespace Weltall
{
	/**
	 * A game's sound as the player hears it: 16-bit stereo at 44,100 Hz, made on demand, with the
	 * game's sound code running at its own rate between the samples. The game hands it what its
	 * ticks ask for from its own thread; the samples are made on the audio thread.
	 */
	class IGameSound
	{
	public:
		static constexpr int32 SampleRate = 44100;

		virtual ~IGameSound() = default;

		/** The next samples, left and right interleaved: Stereo.size() / 2 of them. */
		virtual void Render(std::span<int16> Stereo) = 0;
	};
}
