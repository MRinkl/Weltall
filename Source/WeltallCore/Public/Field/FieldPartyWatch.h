#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	struct FEventContext;

	/**
	 * Whether the party walks as one behind its leader, for the drawing that shows it in the order its player keeps
	 * (FFieldMemberStandIns): only then does it not matter to the story who walks where. The party is together while
	 * the pad walks its first member's actor, every other member's actor is shown and follows it, as the field's own
	 * control step marks its followers each tick, and no message is shown at one of them. A member the scripts have
	 * set apart is itself, and so is everyone else for that while: seated in a cell while the leader walks about,
	 * placed for a scene, acting out a script of its own, speaking, hidden, or without an actor on the map; and so is
	 * a map the leader is not on.
	 *
	 * Apart counts from the tick it is seen; together again only after EndTicks of following, so a member's moment in
	 * a script does not make the party's look flicker. A map's first picture, and the field's first tick back from
	 * elsewhere, show the party as it then is.
	 */
	class FFieldPartyWatch
	{
	public:
		static constexpr int32 EndTicks = 8;

		/** Whether the field's state, as a tick's events left it, has the whole party following its leader and none of it speaking. */
		WELTALLCORE_API static bool Follows(const FEventContext& Context);

		/** Takes the field's state as a tick's events left it. */
		WELTALLCORE_API void Watch(const FEventContext& Context);
		/** A map's load, or the field's return from elsewhere: nothing is known of the party yet. */
		void Reset() { *this = FFieldPartyWatch(); }
		bool Together() const { return bTogether; }

	private:
		bool bTogether = false;
		/** A tick has been watched since the reset. */
		bool bKnown = false;
		/** Ticks in a row the whole party has followed. */
		int32 FollowedTicks = 0;
	};
}
