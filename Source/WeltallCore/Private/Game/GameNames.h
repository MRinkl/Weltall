#pragma once

#include "Foundation/Result.h"
#include "Game/GameState.h"

#include <array>
#include <span>

namespace Weltall
{
	class FMesNameTables;
}

/**
 * The game state's names in their two forms: the message encoding the game shows and plays with,
 * and the glyph indices (16-bit, through the name tables' glyph map) the new-game file and the saves
 * store.
 */
namespace Weltall::GameNames
{
	using FNameSlot = std::array<uint8, FGameState::NameBytes>;

	/**
	 * A slot of up to ten glyph indices, ended early by End, in the message encoding: a glyph map
	 * entry whose lead byte is zero is a one-byte code. Only the slot's bytes are kept, so ten
	 * two-byte glyphs lose their terminator, as in the original. The original leaves stale scratch
	 * bytes after a shorter name's terminator; nothing reads them, and here they are zero.
	 */
	TResult<FNameSlot> FromGlyphIndices(std::span<const uint8> Slot, uint16 End, const FMesNameTables& NameTables);

	/**
	 * A name in the message encoding as glyph indices ended by zeros: codes from LeadByte up take a
	 * second byte, and each code is the first glyph map entry that holds it. A code the map lacks is
	 * stored as 8000h and ends the name, as in the original. A name that fills its slot ends there.
	 */
	FNameSlot ToGlyphIndices(std::span<const uint8> Name, uint8 LeadByte, const FMesNameTables& NameTables);
}
