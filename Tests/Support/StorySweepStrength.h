#pragma once

namespace Weltall
{
	class FGameState;
}

namespace Weltall::Testing
{
	/**
	 * Makes every character and gear of a game state far stronger than the story's battles ask for: full points at
	 * their caps, attack, defense and ether well above any enemy's, a gear's armor, output and fuel likewise. A
	 * battle fought by a random pad is then won, and the play goes on past it; what the story's scripts read (who is
	 * in the party, what it carries, what it has done) is left as it was.
	 */
	void StrengthenParty(FGameState& State);
}
