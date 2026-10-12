#pragma once

#include "Foundation/Types.h"

#include <array>

namespace Weltall
{
	/** A kind of item the battle can drop, as the inventory lists them. */
	namespace BattleDropKinds
	{
		inline constexpr uint8 Weapon = 0;
		inline constexpr uint8 Accessory = 1;
		inline constexpr uint8 Item = 2;
		inline constexpr uint8 GearWeapon = 3;
		inline constexpr uint8 GearPart = 4;
	}

	/** Drops of one kind and id, grouped as the result screen lists them. */
	struct FBattleDropGroup
	{
		uint8 Kind = 0;
		uint8 Id = 0;
		uint8 Count = 0;
	};

	/** What the battle's result works out for the party, the game state and the result screen. */
	struct FBattleResultWork
	{
		static constexpr int32 EnemyCount = 8;
		static constexpr int32 StatCount = 7;

		/** The defeated enemies' EXP, and a penalty that takes a quarter of it per unit (nothing sets it). */
		uint32 TotalExp = 0;
		int8 ExpPenalty = 0;
		/** The enemies whose rewards count (bit i: enemy slot 3 + i), and the gold they gave. */
		uint16 Defeated = 0;
		uint32 GoldGained = 0;
		/** Each party slot's EXP and ether EXP gain, as the screen counts it up. */
		std::array<uint32, 3> ExpGains{};
		std::array<uint32, 3> EtherExpGains{};
		/** Each enemy's drop: its kind (kept from the last drop rolled) and its id (0 none). */
		std::array<uint8, EnemyCount> DropKinds{};
		std::array<uint8, EnemyCount> DropIds{};
		/** The drops grouped as the screen lists and gives them. */
		std::array<FBattleDropGroup, EnemyCount> DropGroups{};
		/** Each party slot's Deathblow learned (its number, 0 for none or the first) and ability learned (its number + 1). */
		std::array<uint8, 3> LearnedDeathblows{};
		std::array<uint8, 3> LearnedAbilities{};
		/** Each party slot's level and ether level before the EXP, and the levels gained. */
		std::array<std::array<uint8, 2>, 3> StartLevels{};
		std::array<std::array<uint8, 2>, 3> LevelsGained{};
		/** Each party slot's attack, hit, defense, evade, ether, ether defense and agility after the level-ups. */
		std::array<std::array<uint8, StatCount>, 3> NewStats{};
	};

	/**
	 * The result's routine under way (BattleResult.h): where it stands, and what its screen shows, which the HUD's
	 * result set draws and the input step counts by.
	 */
	struct FBattleResultRun
	{
		/** The result has begun (it stays so once it is over), and its routine still runs. */
		bool bBegun = false;
		bool bRunning = false;
		/** The routine's place between two frames, and the panels' build step. */
		uint8 Pc = 0;
		uint8 Step = 0;
		/** The party slot the level-ups' part is at, the skill its messages have come to, and whether among the abilities. */
		uint8 Slot = 0;
		uint8 Skill = 0;
		bool bAbilities = false;
		/** The screen runs: a won battle that gave rewards, which neither its event nor its formation holds back; and it holds its work. */
		bool bScreen = false;
		bool bScreenWork = false;
		/** What the screen shows: the party's panels, a member's level-up, a learned skill's message, the items and gold. */
		bool bPanelsShown = false;
		bool bLevelUpShown = false;
		bool bLearnedShown = false;
		bool bSpoilsShown = false;
		/** The menu's windows the screen shows: the items', the level-ups', the gold's. */
		std::array<bool, 3> bWindowShown{};
		/** The EXP counts up a step at a time; each panel's count and ether count are done. */
		bool bCounting = false;
		std::array<bool, 3> bExpCounted{};
		std::array<bool, 3> bEtherCounted{};
		/** The panels show the ether level and its EXP in rows of their own (the story flag that joins them is clear). */
		bool bEtherRows = false;
		/** The fanfare has played: once a screen, at the first level-up or learned skill. */
		bool bFanfarePlayed = false;
		/** The battle's end as the screen found it, cleared while it runs and put back as it closes. */
		uint8 SavedEnd = 0;
	};
}
