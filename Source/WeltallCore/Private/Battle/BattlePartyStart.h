#pragma once

namespace Weltall
{
	struct FBattleContext;

	/**
	 * Each party member at the battle's start: the figures the result screen compares with, what the
	 * equipment adds, the gear's parts and multiplier, the costs Ether Doubler and Economether change,
	 * Chu-Chu's giant form, and the game state's changes (Infinity at level 50, the Xenogears' and the
	 * Yggdra's commands, the second Citan's sword).
	 */
	void InitPartyCombatants(FBattleContext& Context);

	/** A special party's two adult Emeraldas: 100 HP, a fixed hit and evade, and only attacks. */
	void InitSpecialPartyMembers(FBattleContext& Context);
}
