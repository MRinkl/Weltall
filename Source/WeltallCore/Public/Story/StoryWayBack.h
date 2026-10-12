#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Game/WayBackPoint.h"

#include <string_view>
#include <vector>

namespace Weltall
{
	class FGameState;

	/**
	 * Where a game of the expanded second disc goes on in the original's: for each stretch of the expanded play, by the
	 * story's progress there, the original's point that matches what the player has seen and played (the chair that
	 * narrates what comes next, at its place), written as text:
	 *
	 *   way back
	 *   story 253 to map 617 entrance 2 story 252         a game whose story is at 253 goes on at map 617 by its
	 *   story 254-257 to map 618 entrance 0 story 252     entrance 2, the story at 252; and a range likewise
	 *
	 * A ';' begins a comment. A save of such a game is the original's at that point, keeping the game's own place
	 * beside it for the port (SaveWrite.h); turning the expansion off puts the game there.
	 */
	class FStoryWayBack
	{
	public:
		/** Reads the text; the error names the line. */
		WELTALLCORE_API static TResult<FStoryWayBack> Parse(std::string_view Text);

		/** The original's point for a game whose story is at Story, null where the expanded play is the original's own. */
		WELTALLCORE_API const FWayBackPoint* For(uint16 Story) const;

		/** The point for State: a game of the expanded second disc whose story a row names; null for any other. */
		WELTALLCORE_API const FWayBackPoint* For(const FGameState& State) const;

	private:
		struct FRow
		{
			uint16 First = 0;
			uint16 Last = 0;
			FWayBackPoint Point;
		};

		std::vector<FRow> Rows;
	};
}
