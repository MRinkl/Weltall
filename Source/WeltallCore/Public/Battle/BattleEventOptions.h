#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	/** Fixes of the battle events' broken effects: each is on unless the original's behavior is asked for. */
	struct FBattleEventOptions
	{
		/**
		 * A start-and-wait (04, 05) starts the named actor's script unless it was that actor's the running actor
		 * last started; the original judges by the script's number alone, so after a start of the same number
		 * on another actor it starts nothing and waits on nothing.
		 */
		bool bWaitedStartNamesActor = true;
		/**
		 * An event battle lost without its defeat scripted ends as any lost battle (the mode's result Lost);
		 * the original sorts that end by a stale value, which from the field matches no end: the mode's
		 * result stays as the last program left it.
		 */
		bool bUnscriptedDefeatIsLost = true;

		/** Every fix off: the original's behavior throughout. */
		static constexpr FBattleEventOptions Original()
		{
			FBattleEventOptions Options;
			Options.bWaitedStartNamesActor = false;
			Options.bUnscriptedDefeatIsLost = false;
			return Options;
		}
	};
}
