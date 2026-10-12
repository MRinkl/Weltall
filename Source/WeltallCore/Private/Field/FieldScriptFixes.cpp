#include "Field/FieldScriptFixes.h"

#include <array>

namespace Weltall
{
	namespace
	{
		struct FScriptFix
		{
			int32 Map = 0;
			uint16 Offset = 0;
			std::array<uint8, 1> Was{};
			std::array<uint8, 1> Now{};
			bool FEventOptions::*Option = nullptr;
		};

		/**
		 * The hotel's head count (actor 14's script 4) asks twice whether party slot 1 is empty, so a party
		 * of two pays for three; its second If names slot 2's variable instead.
		 */
		constexpr FScriptFix Fixes[] = {
			{104, 0x3A0, {0x40}, {0x42}, &FEventOptions::bHotelCountsWholeParty},
			// FE 62 (volume, channel) at 0066, 0080, 00BA and 00C6 names volume 0 or 1 on channel 30 for the
			// sounds just started on channels 0 and 1: volume 30 on them, the two operands' low bytes swapped.
			{272, 0x68, {0x00}, {0x1E}, &FEventOptions::bShipSoundsAtTheirVolume},
			{272, 0x6A, {0x1E}, {0x00}, &FEventOptions::bShipSoundsAtTheirVolume},
			{272, 0x82, {0x00}, {0x1E}, &FEventOptions::bShipSoundsAtTheirVolume},
			{272, 0x84, {0x1E}, {0x00}, &FEventOptions::bShipSoundsAtTheirVolume},
			{272, 0xBC, {0x01}, {0x1E}, &FEventOptions::bShipSoundsAtTheirVolume},
			{272, 0xBE, {0x1E}, {0x01}, &FEventOptions::bShipSoundsAtTheirVolume},
			{272, 0xC8, {0x00}, {0x1E}, &FEventOptions::bShipSoundsAtTheirVolume},
			{272, 0xCA, {0x1E}, {0x00}, &FEventOptions::bShipSoundsAtTheirVolume},
			// Actor 11's script 7 fades channel 12 (FE 8C at 0944) where the room's sound plays on channel 3.
			{670, 0x948, {0x0C}, {0x03}, &FEventOptions::bMemoryAmbienceFades},
		};
	}

	void ApplyFieldScriptFixes(int32 Map, FEventScripts& Scripts, const FEventOptions& Options)
	{
		for (const FScriptFix& Fix : Fixes)
		{
			if (Fix.Map == Map && Options.*Fix.Option)
			{
				Scripts.Replace(Fix.Offset, Fix.Was, Fix.Now);
			}
		}
	}
}
