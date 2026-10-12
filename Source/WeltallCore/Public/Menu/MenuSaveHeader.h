#pragma once

#include "Foundation/Types.h"

#include <array>
#include <span>

namespace Weltall
{
	class FMenuResources;

	/** The header of a save's file on a memory card, before its game data. */
	inline constexpr std::size_t SaveHeaderSize = 0x100;

	/**
	 * The header the game's Save writes before a save's data on a memory card: "SC", an icon of one frame,
	 * one block; the title "ＸＥＮＯＧＥＡＲＳ／Ｎｏ．" with the slot's number (Slot from 0) in two full-width
	 * digits, a full-width space and the place the save was made at, which the story's progress in Data
	 * names (the 17th directory's first file, a line a value); the save icon's colours and pixels.
	 */
	WELTALLCORE_API std::array<uint8, SaveHeaderSize> MenuSaveHeader(const FMenuResources& Resources, int32 Slot, std::span<const uint8> Data);
}
