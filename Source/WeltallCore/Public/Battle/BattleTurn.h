#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	struct FBattleContext;

	/** The rules a turn runs besides its action: its start and end, and the commands without a handler. */

	/**
	 * A turn's start: the actor's timed statuses (Slow, Sleep, Can't use Ether, the buffs and element
	 * modes; a gear's own) count down one turn. Returns the bits of those that ran out, for the show;
	 * 0 for a gear.
	 */
	WELTALLCORE_API uint16 TickStatusDurations(FBattleContext& Context, uint8 Slot);

	/**
	 * A turn's end: what the actor loses to Poison, EP loss, a fuel leak and the Booster, each applied
	 * as its own record (results 8, 9 and 10), then shown on the actor when any of them applies (an
	 * amount may be 0); the show then takes frames (IBattleShow::IsBlocking).
	 */
	WELTALLCORE_API void ApplyTurnDrains(FBattleContext& Context, uint8 Slot);

	/**
	 * A gear's turn start, as its command menu opens: the figures the cockpit shows (fuel costs, the
	 * Charge amount, attack, lamps, the Infinity chance), the Booster off on an empty tank, Infinity
	 * counting down, and at attack level 3 the roll that may start it.
	 */
	WELTALLCORE_API void PrepareGearTurn(FBattleContext& Context, uint8 Slot);

	/** Guard (and a gear's Charge): the next wait from command 0, and half of the hits halved until the member's next turn. */
	WELTALLCORE_API void DefendInBattle(FBattleContext& Context, uint8 Slot);

	/** The member's next turn begins: its guard ends. */
	WELTALLCORE_API void EndBattleDefence(FBattleContext& Context, uint8 Slot);

	/** An escape: one chance in two; the party's HP, EP, counters and gears go back to the game state when it works. */
	WELTALLCORE_API bool TryBattleEscape(FBattleContext& Context);

	/**
	 * The party back into the game state after an escape: HP (1 when knocked out; Chu-Chu's from her
	 * giant form), EP, the Deathblow counters and the exact-kill count, and the gears' HP and fuel.
	 */
	WELTALLCORE_API void WriteBackBattleParty(FBattleContext& Context);
}
