#include "Sound/GameSoundSystem.h"

#include <algorithm>

namespace Weltall
{
	FGameSoundSystem::FGameSoundSystem(const FSoundDriverTables& Tables, const FSoundDriverOptions& Options)
		: Driver(Spu, Tables, Options)
	{
	}

	FLockedGameSound FGameSoundSystem::Lock()
	{
		return FLockedGameSound(Mutex, Driver, Spu);
	}

	void FGameSoundSystem::Render(std::span<int16> Stereo)
	{
		const std::size_t Frames = Stereo.size() / 2;
		std::size_t Made = 0;
		while (Made < Frames)
		{
			const std::lock_guard<std::mutex> Guard(Mutex);
			if (Clock.IsInterruptDue())
			{
				Driver.Interrupt();
				Clock.RunInterrupt();
			}
			const std::size_t Count = std::min(Frames - Made, static_cast<std::size_t>(Clock.SamplesToInterrupt()));
			Spu.Render(Stereo.subspan(Made * 2, Count * 2));
			Clock.MakeSamples(static_cast<int32>(Count));
			Made += Count;
		}
	}
}
