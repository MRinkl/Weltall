#pragma once

namespace Weltall
{
	struct FBattleContext;

	/**
	 * Who is still in the fight, after each turn: the living mask (in the battle, not knocked out,
	 * not hidden or escaped; enemies that react while knocked out still count), the knocked out's HP
	 * put to 0 and their turns taken away for good, the end when one side is gone (a lost battle
	 * wins over a won one), and when everyone left sleeps, the first sleeper woken.
	 */
	WELTALLCORE_API void CheckBattleEnd(FBattleContext& Context);

	/**
	 * Whether every party member in the battle is out (knocked out or petrified, as the end's check counts them): from
	 * the blow that fells the last, before the turn's end finds the battle lost.
	 */
	WELTALLCORE_API bool IsPartyDown(const FBattleContext& Context);
}
