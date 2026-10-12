#pragma once

#include "Game/GameState.h"

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
}

namespace Weltall::Testing
{
	/**
	 * Copies a new game's party bytes from the disc's new-game file into a game state, as the field
	 * oracles lay a map out: the characters who have joined, the party, whether each slot rides its
	 * gear, and each character's gear. False when the disc lacks the file.
	 */
	bool CopyNewGameParty(const FCdImage& Image, const FGameFileIndex& Index, FGameState& State);
}
