#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	struct FBattleContext;

	/**
	 * The steps of the battle's rules the public entry points are made of. Each works on the actor,
	 * target and command the context's action points at, as the game's steps do.
	 */
	namespace BattleSteps
	{
		/** The hit roll: 1 hit, 2 halved, 3 miss, 5 sure hit (the element step may make it 4, absorbed). */
		uint8 RollHit(FBattleContext& Context);
		/** The attack and defense of an on-foot hit, before the buffs and the element. */
		int16 CalcAttack(FBattleContext& Context);
		int16 CalcDefense(FBattleContext& Context);
		/** The element's scale on the attack (the defense kept), and the absorbing guards' hit 4. */
		void ApplyElement(FBattleContext& Context, uint16& Attack, uint16& Defense, uint8& Hit);
		/** Chu-Chu's power: half again per pinch of Fei's and Bart's, and a chance of half or twice more. */
		void ChuChuPowerBonus(FBattleContext& Context, uint16& Power);
		/** Elly's ether may fail: a third of the damage. */
		void RollEtherFailure(FBattleContext& Context);

		/** The on-foot handlers, by the command's handler number. */
		void HandlerDamage(FBattleContext& Context);
		void HandlerHeal(FBattleContext& Context);
		void HandlerStatus(FBattleContext& Context);
		void HandlerDrain(FBattleContext& Context);
		void HandlerFixedDamage(FBattleContext& Context);
		void HandlerCure(FBattleContext& Context);
		void HandlerRevive(FBattleContext& Context);
		void HandlerRestoreGearHp(FBattleContext& Context);
		/** After a landed hit: the actor's weapon's status, or the command's buffs taken away. */
		void ApplyWeaponStatus(FBattleContext& Context);
		void DispelBuffs(FBattleContext& Context);

		/** A status's roll and whether the target takes it; true when it does (or a buff pair cancels). */
		bool TryInflictStatus(FBattleContext& Context, uint8 Chance, uint8 Group, uint16 Bits);
		/** The turns a status lasts, set on a slot, by the action's actor and target. */
		void SetStatusDuration(FBattleContext& Context, uint8 Slot, uint8 Group, uint16 Bits, uint8 Turns);
		/**
		 * The same by the actor and the target given: Ether Doubler's by the actor, a buff's trade-offs by the
		 * target (the action's when none is given, looked up for a buff only).
		 */
		void SetStatusDurationBy(FBattleContext& Context, const FBattleCombatant& Actor, const FBattleCombatant* Target, uint8 Slot, uint8 Group,
			uint16 Bits, uint8 Turns);
		/** The battle message that names a status just taken. */
		void SetStatusMessage(FBattleContext& Context, uint8 Group, uint16 Bits);
		/** Whether one more party member may play dead. */
		bool CanPlayDead(const FBattleContext& Context);

		/** Citan's party-wide ether: his ether commands' targeting while the status holds, and back. */
		void ApplyEtherTargetAll(FBattleContext& Context, uint8 Slot);
		void ResetEtherTargetAll(FBattleContext& Context, uint8 Slot, bool bOnlyForEther);

		/** A slot leaves its group's place on the stage. */
		void LeaveGroup(FBattleContext& Context, uint8 Slot);
		/** A combatant on foot walks into the target's group, taking its first free place, when the group has room. */
		void MoveToTargetGroup(FBattleContext& Context, uint8 Mover, uint8 Target);
		/** A combatant in a gear moves to the target's group, when no gear stands there. */
		void MoveGearToTargetGroup(FBattleContext& Context, uint8 Mover, uint8 Target);
		/**
		 * The route an approach walks from the actor's place to the target's group, by the stage's routes;
		 * true when the actor is Billy, who shoots from where he stands.
		 */
		bool PlanApproachPath(FBattleContext& Context, uint8 Actor, uint8 Target);

		/**
		 * An action by or through a gear: the gear table's command (the enemies' for an enemy), each target
		 * in slot order with a plain attack aimed at someone on foot turned into its on-foot form, the gears'
		 * handlers, the status a landed hit carries, the attack level and the Deathblows' count for Infinity,
		 * then Billy's gear's ammunition and the element for the show.
		 */
		void GearAct(FBattleContext& Context);
		/** A gear's hit roll: 1 hit, 2 guarded, 3 miss (the element step may make it 4). */
		uint8 GearRollHit(FBattleContext& Context);
		/** A gear's attack and its target's defense, with the ether amplifier, the armor break and the buffs. */
		uint16 GearCalcAttack(FBattleContext& Context);
		uint16 GearCalcDefense(FBattleContext& Context);
		/** The gears' damage handler. */
		void GearDamage(FBattleContext& Context);
		/** A status a gear action carries onto a target in a gear: the command's (duration its power), or the gear weapon's (5). */
		bool GearApplyStatus(FBattleContext& Context, bool bWeapon);
	}
}
