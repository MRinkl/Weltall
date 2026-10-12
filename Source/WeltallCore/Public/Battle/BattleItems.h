#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	struct FBattleContext;

	/**
	 * An item used in battle by the member in slot User: the item menu's use, with the knocked-out mask
	 * emptied, the living noted and the item's targets, then its effect on each target in slot order.
	 */
	WELTALLCORE_API void PerformBattleItem(FBattleContext& Context, uint8 User, uint16 Targets, uint8 Item);

	/**
	 * An item's effect on the action's targets: heals (whole, even on gears), cures, a revive, buffs,
	 * element modes, immunities, Play Dead, and the specials. A target in a gear takes only the heals and
	 * the specials. The show's number for the item follows. The statuses' turns are set with the last
	 * action's actor and target, as the game does, or with the fix (`FBattleRuleOptions::bItemBuffsByItsUser`)
	 * with the item's user and each target.
	 */
	WELTALLCORE_API void UseBattleItem(FBattleContext& Context, uint8 User, uint8 Item);
}
