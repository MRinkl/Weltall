#pragma once

#include "Sound/GameSound.h"

#include <vector>

namespace Weltall
{
	/**
	 * A game's sound kept going where nothing plays it (no audio device): the samples each stretch of
	 * time makes are made and dropped, so its songs and effects run on, and end, as they would be heard.
	 * It allocates nothing as it runs.
	 */
	class FUnheardGameSound
	{
	public:
		WELTALLCORE_API explicit FUnheardGameSound(IGameSound& InSound);

		/** Lets Seconds of time pass. */
		WELTALLCORE_API void Advance(double Seconds);

	private:
		IGameSound& Sound;
		std::vector<int16> Scratch;
		/** The samples due and not made yet, a fraction of one. */
		double Owed = 0.0;
	};
}
