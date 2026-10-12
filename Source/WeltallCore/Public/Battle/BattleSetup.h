#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	class FBattleScene;
	class FBattleStage;
	class FBattleTables;
	struct FBattleContext;
	struct FBattleFormation;

	/**
	 * Sets a battle up as the Battle program does before its opening: the party chosen from the
	 * game state's (those who have joined; or the leader and two adult Emeraldas in gear 17) with
	 * their records, gears and command tables, the enemies from the scene, where everyone stands on
	 * the stage, the party's figures at the start and what their equipment adds, the items the
	 * battle lists, the first waits and the battle's random order (with the game's rand()), the
	 * commands the formation switches off, every slot's default target, and whether the battle is
	 * already over. The context's game state, random numbers, show, program tables, arctangents and
	 * whether the field's gears are locked must be set; the rest of it is overwritten. The enemies'
	 * scripts are read from the scene's file, which must outlive the battle. Faults stop the setup as
	 * they stop the rules.
	 */
	WELTALLCORE_API void SetUpBattle(FBattleContext& Context, const FBattleFormation& Formation, const FBattleTables& Tables,
		const FBattleScene& Scene, const FBattleStage& Stage);

	/** The setup's stages, in the order the Battle program's start runs them (between them, the stage's file is read). */
	enum class EBattleSetupStage : uint8
	{
		/** The party chosen, its combatants and every command table. */
		Combatants,
		/** The slots reset, the formation placed on the stage, the enemies from the scene, the party's start. */
		Placement,
		/** The items listed, the first waits and the battle's random order, the menus and the default targets. */
		Order,
	};

	/**
	 * The context made fresh for a battle as the program's start clears the battle's memory: everything but
	 * the services the setup keeps (the game state, random numbers, show, program tables, arctangents,
	 * whether the gears are locked, the event's options), with the formation and the scene. The stage's
	 * layout comes with its file, between the setup's first two stages (the context's Stage).
	 */
	WELTALLCORE_API void BeginBattleSetup(FBattleContext& Context, const FBattleFormation& Formation, const FBattleScene& Scene);

	/** One of the setup's stages, as SetUpBattle runs them in order after BeginBattleSetup (then the end check); none after a fault. */
	WELTALLCORE_API void RunBattleSetupStage(FBattleContext& Context, EBattleSetupStage Stage, const FBattleTables& Tables, const FBattleScene& Scene);

	/**
	 * The end of the party's setup, after the battle's opening: what Play Dead gives back, the gears'
	 * agility on the stage's ground, and the values the battle forces into the game state (Maria's
	 * agility and ability, the gears' multipliers before a point in the story).
	 */
	WELTALLCORE_API void FinishBattlePartySetup(FBattleContext& Context);
}
