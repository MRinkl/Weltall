#pragma once

namespace Weltall
{
	/** The arena's fixes: each on unless the original's behavior is asked for. */
	struct FBattlingOptions
	{
		/** The computer's moves from afar are picked by their own chain state; the original pairs each with the next. */
		bool bRangedMovesByOwnState = true;
		/** Practice's easy, normal and hard computer styles play at the level they name; the original plays all three at the menus' level. */
		bool bPracticeComputerAtStyleLevel = true;
		/** Practice's frame rates are the ones their labels name; the original skips whole ticks, running slower than they say. */
		bool bPracticeFrameRateAsLabelled = true;
	};
}
