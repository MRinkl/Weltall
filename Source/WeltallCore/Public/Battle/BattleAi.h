#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	struct FBattleContext;

	/**
	 * The enemies' AI. Each enemy runs scripts of its scene's file: 4-byte words, conditions (80h-9Bh)
	 * guarding blocks of commands (below 80h) that compute into the enemy's variables and fill the action
	 * queue, which is then carried out entry by entry into records for the show (acts through the rules,
	 * moves between the stage's groups, messages, waits, animations, transformations, escapes).
	 */

	/** An enemy's turn: the queue and the show's records emptied, then its turn script. */
	WELTALLCORE_API void RunBattleTurnScript(FBattleContext& Context, uint8 Slot);

	/**
	 * An enemy's on-hit script, after a party member's combo hit on it (while it stands, or reacts while
	 * knocked out), its queue carried out after the attacker's records when the script filled it.
	 * Returns true when the script asked the combo to end.
	 */
	WELTALLCORE_API bool RunBattleHitScript(FBattleContext& Context, uint8 Slot);

	/** A party turn's reactions begin: the enemies' scripts fill one queue from its first entry on. */
	WELTALLCORE_API void BeginBattleReactions(FBattleContext& Context);

	/**
	 * Enemy Enemy's reaction script, when the party turn aimed at it and it has one: it becomes the
	 * acting slot, and when it stands (or reacts while knocked out) the show's records are emptied and the
	 * script adds to the queue where the one before stopped. Returns true when it ran: its actions follow
	 * (PrepareBattleEnemyActions, a frame, BeginBattleEnemyPlayback, the show's frames, RunBattleEnemyActions,
	 * the show), then the battle event's hook.
	 */
	WELTALLCORE_API bool RunBattleReactionScript(FBattleContext& Context, uint8 Enemy);

	/** The reactions end: the party turn's acting slot back. */
	WELTALLCORE_API void EndBattleReactions(FBattleContext& Context);

	/** A party action's hit on an enemy, noted in its AI: who, with what kind of command, and whether its target fell. */
	WELTALLCORE_API void RecordBattleHit(FBattleContext& Context, uint8 Actor, uint8 Target);

	/** An enemy's actions begin: the show's records and the turn's totals emptied, the first record the actor's. */
	WELTALLCORE_API void PrepareBattleEnemyActions(FBattleContext& Context, uint8 Slot);

	/** An enemy's actions, a frame after they began: the playback starts (the show then takes frames: IBattleShow::IsBlocking). */
	WELTALLCORE_API void BeginBattleEnemyPlayback(FBattleContext& Context, uint8 Slot);

	/** An enemy's actions once the playback has started: the queue is carried out and its end recorded; the turn then waits for the show. */
	WELTALLCORE_API void RunBattleEnemyActions(FBattleContext& Context, uint8 Slot);

	/** Carries the action queue out for an enemy: every entry in order, each into records for the show. */
	WELTALLCORE_API void ExecuteBattleAiQueue(FBattleContext& Context, uint8 Actor);

	/** The records that end an enemy's actions: a gear's closing sequence, then the end; the menu's sounds go off. */
	WELTALLCORE_API void FinishBattleAiQueue(FBattleContext& Context, uint8 Actor);

	/** The slot the playback readies next: the one with the lowest wait in the battle's order, other than the actor. */
	WELTALLCORE_API uint8 FindNextReadySlot(const FBattleContext& Context, uint8 Actor);
}
