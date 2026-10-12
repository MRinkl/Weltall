#pragma once

#include "Foundation/Types.h"

#include <span>

namespace Weltall
{
	/**
	 * The kernel's fade of 15-bit colours toward black, as scripts dim CLUTs with it: each channel of each
	 * colour is scaled by Factor / 32 through the GTE (Factor past 32 counts as 32), the semi-transparency
	 * bit kept, and a colour that was not 0 but fades to black becomes 1 so that it stays opaque. Faded
	 * takes as many colours as Colors holds.
	 */
	WELTALLCORE_API void FadeClutColors(std::span<const uint16> Colors, int32 Factor, std::span<uint16> Faded);

	/**
	 * The kernel's blend of 15-bit colours from From toward To by T / 32 (T past 32 counts as 32): each
	 * channel moved by its difference scaled through the GTE, the scaled difference masked to the
	 * channel's bits and added, so a channel moving down carries into the next; the semi-transparency
	 * bit is lost. Blended takes as many colours as From holds.
	 */
	WELTALLCORE_API void LerpClutColors(std::span<const uint16> From, std::span<const uint16> To, int32 T, std::span<uint16> Blended);

	/**
	 * The kernel's tint of 15-bit colours: each colour's channels taken whole (Mode 0), halved (1), quartered (2) or as
	 * their grey (3, the channels' average), Red, Green and Blue added in the channels' own units and each clamped to its
	 * channel as a signed 16-bit sum, then moved toward the colour of Targets by T / 32 (T past 32 counts as 32) as
	 * LerpClutColors moves it, with the semi-transparency bit set. A colour of 0 stays transparent: Tinted keeps what it
	 * held there. Tinted and Targets take as many colours as Colors holds; the modes past 3 are never asked of it.
	 */
	WELTALLCORE_API void TintClutColors(std::span<const uint16> Colors, std::span<const uint16> Targets, int32 Red, int32 Green, int32 Blue, int32 Mode,
		int32 T, std::span<uint16> Tinted);
}
