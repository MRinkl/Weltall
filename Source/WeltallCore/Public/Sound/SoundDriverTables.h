#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <array>

namespace Weltall
{
	class FPsxExecutable;

	/** One of the sound library's reverb presets: the work area's start and the 32 reverb registers. */
	struct FSpuReverbPreset
	{
		/** In 8-byte units. */
		uint16 Start = 0;
		std::array<uint16, 32> Registers{};
	};

	/** The tables the game's sound driver and the sound library linked with it compute with, as the executable holds them. */
	struct FSoundDriverTables
	{
		static constexpr int32 ReverbTypes = 10;

		/** 8192 * 2^(n / 3072) for n = semitone * 256 + fraction: a pitch within an octave. */
		std::array<int16, 12 * 256> Pitches{};
		/** Per semitone, its octave (high nibble) and note (low nibble); past key 116 the bytes that follow the table. */
		std::array<uint8, 128> Octaves{};
		/** Per command 80h-FFh, the bytes the driver steps over when it looks ahead for the next note. */
		std::array<uint8, 128> CommandLengths{};
		/** The sound RAM the driver reserves for each reverb type's work area. */
		std::array<uint32, ReverbTypes> ReverbAreaSizes{};
		std::array<FSpuReverbPreset, ReverbTypes> ReverbPresets{};
		/** The block the sound library's start-up leaves every voice playing. */
		std::array<uint8, 16> SilentBlock{};

		WELTALLCORE_API static TResult<FSoundDriverTables> Read(const FPsxExecutable& Executable);
	};
}
