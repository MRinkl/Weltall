#pragma once

#include "Foundation/Types.h"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace Weltall
{
	/** How an XA-ADPCM sector is coded: the fourth byte of its subheader. */
	struct FXaCodingInfo
	{
		bool bStereo = false;
		/** 18900 Hz instead of 37800 Hz. */
		bool bHalfRate = false;
		/** 8-bit samples instead of 4-bit ones. */
		bool bEightBit = false;

		WELTALLCORE_API static FXaCodingInfo FromSubheader(uint8 CodingInfo);

		int32 SampleRate() const { return bHalfRate ? 18900 : 37800; }
	};

	/** The two previous samples a channel's filter reads; they carry over from sector to sector. */
	struct FXaChannelHistory
	{
		int16 Old = 0;
		int16 Older = 0;
	};

	namespace XaSector
	{
		/** A sector's sound: 18 groups of 128 bytes at the start of its 2324 bytes of user data. */
		inline constexpr std::size_t GroupCount = 18;
		inline constexpr std::size_t GroupBytes = 128;
		inline constexpr std::size_t SoundBytes = GroupCount * GroupBytes;
	}

	/**
	 * Decodes one sector's sound and appends it to Out: interleaved left and right samples in stereo.
	 * History holds the left (or mono) channel's filter state, then the right's. UserData must hold at
	 * least XaSector::SoundBytes.
	 */
	WELTALLCORE_API void DecodeXaSector(std::span<const uint8> UserData, FXaCodingInfo Coding,
		std::array<FXaChannelHistory, 2>& History, std::vector<int16>& Out);
}
