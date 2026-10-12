#pragma once

#include "Foundation/Types.h"

#include <optional>

namespace Weltall
{
	struct FEventContext;

	/**
	 * Records the leader's step in the party's trail, as the field does after the leader's layer and
	 * position step: its flags, position, floor, animation, facing and its sprite's motion. Nothing for
	 * another actor or while the party walks as a group. False when the leader has no sprite.
	 */
	WELTALLCORE_API bool RecordLeaderStep(FEventContext& Context, int32 Actor);

	/**
	 * Starts the trail afresh from where the leader stands: every step the leader's present one, and
	 * every index at the start. False when the leader has no sprite.
	 */
	WELTALLCORE_API bool RestartFollowTrail(FEventContext& Context);

	/**
	 * Sets the party on the leader, as the field does once a map's load has placed the leader and when a
	 * member joins while the map runs: the leader, then every walking actor standing for the party's
	 * second or third member, takes its layer and floor step (on a platform at its own height when
	 * bOnPlatform: the original passes a word it never set, which on these paths is never 0 but for a
	 * member standing for actor 0), each member is put where the leader's sprite and record are, and the
	 * trail starts afresh from the leader. The actor that could not be placed, or nothing.
	 */
	WELTALLCORE_API std::optional<int32> PlacePartyOnLeader(FEventContext& Context, bool bOnPlatform);

	/**
	 * The party's other members' tick, after the actors have moved: each member the control step marked
	 * as a follower this tick replays the leader's recorded steps (position, floor, facing, motion,
	 * animation and jumps), the second member 10 steps behind the newest and the third 20, or the
	 * newest one while the followers keep close. A member that has caught up, or whose place in the
	 * trail is not due, stands with its idle animation. While the leader is not the party's first
	 * member, the followers only stand idle. False when a follower has no sprite.
	 */
	WELTALLCORE_API bool UpdatePartyFollowers(FEventContext& Context);
}
