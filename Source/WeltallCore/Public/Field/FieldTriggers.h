#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	struct FEventContext;

	/**
	 * The field's talk and touch checks for the leader, once per tick after the actors have moved (the
	 * field skips them while the scripts hold the player or the party walks as a group). Each other
	 * actor that is not frozen and not the one the leader stands on is tested in turn: a platform the
	 * leader touches this tick, then an actor's box grown by 16 or its reach around its interaction
	 * point, at the height of the leader's body. The action button newly pressed, facing the actor and
	 * no message window open, starts its talk script (one actor a tick); otherwise contact starts its
	 * touch script. The chosen actor turns toward the leader. False when the context has no leader's
	 * script state or no arctangent table.
	 */
	WELTALLCORE_API bool CheckFieldTriggers(FEventContext& Context);
}
