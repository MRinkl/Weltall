#pragma once

#include "Battle/BattleContext.h"
#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <cstddef>
#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;

	/**
	 * The growth and learning table: per character, the Deathblows' learning thresholds and levels, the
	 * stats' targets, the levels that raise AP, the Deathblows and abilities that unlock gear commands and
	 * the abilities' levels; then the EXP each level needs. Read from its file's first section.
	 */
	class FBattleGrowthTable
	{
	public:
		static constexpr std::size_t RecordSize = 0x110;
		static constexpr int32 CharacterCount = 11;
		static constexpr std::size_t ExpTableOffset = 0xBAC;

		WELTALLCORE_API static TResult<FBattleGrowthTable> Load(const FCdImage& Image, const FGameFileIndex& Index);
		WELTALLCORE_API static TResult<FBattleGrowthTable> Parse(std::vector<uint8> Section);

		std::span<const uint8> Bytes() const { return Data; }

	private:
		std::vector<uint8> Data;
	};

	/**
	 * The result's start (the rules of the result's finish): the items used in the battle go back to the
	 * inventory, and a won battle that gives rewards gives them (GiveBattleRewards). True when the result
	 * screen then runs (and gives the drops: GiveBattleDrops).
	 */
	WELTALLCORE_API bool FinishBattleResults(FBattleContext& Context, const FBattleGrowthTable& Growth, uint8 ModeResult);

	/** Whether the battle's end gives rewards: a win that neither the event ended nor a scripted defeat stands for. */
	WELTALLCORE_API bool BattleGivesRewards(const FBattleContext& Context, uint8 ModeResult);

	/**
	 * The result begun: the input and the HUD turn to the result's, and the routine
	 * takes its first frame. Run it with StepBattleResult.
	 */
	WELTALLCORE_API void BeginBattleResult(FBattleContext& Context);

	/**
	 * The result screen alone begun, as the result's routine begins it once the rewards are given: run it with
	 * StepBattleResult, which shows the screen and closes the two windows behind the message boxes.
	 */
	WELTALLCORE_API void BeginBattleResultScreen(FBattleContext& Context);

	/**
	 * The result to its next frame: a second frame, the result's files, the items written back and the rewards;
	 * then, when the screen runs, the party's panels built over six frames, the EXP counted up until it is done
	 * or the player confirms, the new levels, a window for each member that gained a level and a message for each
	 * skill learned, the items and gold (the drops given as they are listed), every stop waiting for the confirm
	 * button; then the two windows behind the message boxes closed, a frame each. False once it is over.
	 */
	WELTALLCORE_API bool StepBattleResult(FBattleContext& Context, const FBattleGrowthTable& Growth, uint8 ModeResult);

	/**
	 * A step of the EXP count (the result's input step, unless paused): each panel's counter takes one from what
	 * is left to add, the ether's too. True when the panels' figures changed and are to be drawn anew; the count
	 * ends once every panel is done.
	 */
	WELTALLCORE_API bool StepBattleResultCount(FBattleContext& Context);

	/** The items the battle listed go back to the inventory with the counts they were left at. */
	WELTALLCORE_API void WriteBackBattleItems(FBattleContext& Context);

	/**
	 * A won battle's rewards: the defeated enemies' EXP and gold, the party's empty slots marked, and
	 * unless the whole party is knocked out the EXP shared and the level-ups, the Deathblows, abilities
	 * and gear commands learned, the AP per turn raised, the Infinity attacks unlocked, the party written
	 * back to the game state and the drops rolled. True when the result screen runs.
	 */
	WELTALLCORE_API bool GiveBattleRewards(FBattleContext& Context, const FBattleGrowthTable& Growth);

	/** The result screen's drops: grouped by kind and id, and added to the inventory's lists (a full list loses one). */
	WELTALLCORE_API void GiveBattleDrops(FBattleContext& Context);
}
