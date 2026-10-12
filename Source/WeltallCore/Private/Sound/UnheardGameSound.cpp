#include "Sound/UnheardGameSound.h"

#include <algorithm>
#include <span>

namespace Weltall
{
	namespace
	{
		/** Made at most this many samples at a time. */
		constexpr std::size_t ChunkFrames = 1024;
	}

	FUnheardGameSound::FUnheardGameSound(IGameSound& InSound) : Sound(InSound), Scratch(ChunkFrames * 2)
	{
	}

	void FUnheardGameSound::Advance(double Seconds)
	{
		if (!(Seconds > 0.0))
		{
			return;
		}
		Owed += Seconds * IGameSound::SampleRate;
		std::size_t Frames = static_cast<std::size_t>(Owed);
		Owed -= static_cast<double>(Frames);
		while (Frames > 0)
		{
			const std::size_t Count = std::min(Frames, ChunkFrames);
			Sound.Render(std::span<int16>(Scratch.data(), Count * 2));
			Frames -= Count;
		}
	}
}
