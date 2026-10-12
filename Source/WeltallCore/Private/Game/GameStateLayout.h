#pragma once

#include "Foundation/Types.h"

#include <array>
#include <cstddef>

namespace Weltall::GameStateLayout
{
	/** The characters' records, by character id. */
	inline constexpr std::size_t CharactersOffset = 0x26C;
	inline constexpr std::size_t CharacterSize = 0xA4;
	inline constexpr int32 CharacterCount = 11;

	/** The gears' records, by gear id. */
	inline constexpr std::size_t GearsOffset = 0x978;
	inline constexpr std::size_t GearSize = 0xA4;
	inline constexpr int32 GearCount = 20;

	/** What each character has learned: Deathblows, abilities, gear Deathblows... */
	inline constexpr std::size_t LearnedSkillsOffset = 0x16C0;
	inline constexpr std::size_t LearnedSkillsSize = 0x20;

	inline constexpr std::size_t JoinedCharactersOffset = 0x1D30;
	inline constexpr std::size_t CharacterMaskOffset = 0x1D32;
	inline constexpr std::size_t PartyOffset = 0x1D34;

	/** One of the inventory's lists: its slots' counts, then their item numbers, each a byte a slot. */
	struct FInventoryList
	{
		std::size_t Counts;
		std::size_t Items;
		std::size_t Slots;
	};

	/** Numbered as the high byte of the scripts' item codes. */
	inline constexpr std::array<FInventoryList, 5> InventoryLists = {{
		{0x1F90, 0x2026, 150},
		{0x1D38, 0x1D9C, 100},
		{0x1E00, 0x1EC8, 200},
		{0x20BC, 0x2120, 100},
		{0x2184, 0x221A, 150},
	}};

	/** Where the world map keeps the party or the vehicle it travels in (the Yggdrasil): x, y, z and its direction, four words. */
	inline constexpr std::size_t WorldMapPositionOffset = 0x182C;
	/** The world map's flags the field's scripts read and set. */
	inline constexpr std::size_t WorldMapFlagsOffset = 0x1834;
	/** Where the world map keeps its sea object (the Thames): x and z, two words. */
	inline constexpr std::size_t SeaObjectPlaceOffset = 0x1844;
	/** Where the world map keeps its air object (Shevat): x and z, each a fraction then the whole units; these are the whole ones. */
	inline constexpr std::size_t AirObjectXOffset = 0x184E;
	inline constexpr std::size_t AirObjectZOffset = 0x1852;
	inline constexpr std::size_t SavedDiscOffset = 0x19D4;
	/** Billy's guns' ammunition counts, by ammunition. */
	inline constexpr std::size_t AmmunitionCountsOffset = 0x2286;
	/**
	 * A word of story flags (bit 15 the normal EXP rules, bit 14 the 7-AP tier and Infinity, bit 13
	 * character 9 made from another, bit 12 character 10 made: the adult Emeralda in Crescens);
	 * Billy's gears' ammunition counts are the bytes from it on.
	 */
	inline constexpr std::size_t StoryFlagsOffset = 0x22B6;
	/** The arena's gears player 1 has beaten, a bit each, and the arena's settings word after them. */
	inline constexpr std::size_t ArenaDefeatedGearsOffset = 0x2344;
	inline constexpr std::size_t ArenaDefeatedGearsSize = 8;
	inline constexpr std::size_t ArenaSettingsOffset = 0x234C;
	/** Flags of things the battle does only once. */
	inline constexpr std::size_t BattleOnceFlagsOffset = 0x2355;
	/** Where a save keeps the battle scripts' globals. */
	inline constexpr std::size_t SavedBattleGlobalsOffset = 0x2324;
	/** A byte for each party slot: 1 when its member rides its gear. */
	inline constexpr std::size_t PartyInGearOffset = 0x22B1;
	/** Bit c set: character c cannot be moved in Member Change. */
	inline constexpr std::size_t LockedCharactersOffset = 0x2318;
	inline constexpr std::size_t PlaceOffset = 0x231A;
	inline constexpr std::size_t CameraYawOffset = 0x231C;
	inline constexpr std::size_t WorldMapEntranceOffset = 0x231E;
	/** The world map's scene, where the field leaves the entrance for the next map. */
	inline constexpr std::size_t SceneOffset = 0x2320;
	inline constexpr std::size_t SongOffset = 0x2322;

	namespace Character
	{
		inline constexpr std::size_t WeaponAttack = 0x04;
		/** Hits that dealt exactly the target's remaining HP. */
		inline constexpr std::size_t ExactKills = 0x3A;
		inline constexpr std::size_t Exp = 0x3C;
		inline constexpr std::size_t EtherExp = 0x40;
		inline constexpr std::size_t ExpToNext = 0x44;
		inline constexpr std::size_t EtherExpToNext = 0x48;
		inline constexpr std::size_t Hp = 0x4C;
		inline constexpr std::size_t MaxHp = 0x4E;
		inline constexpr std::size_t Ep = 0x50;
		inline constexpr std::size_t MaxEp = 0x52;
		inline constexpr std::size_t Agility = 0x5A;
		inline constexpr std::size_t Level = 0x62;
		inline constexpr std::size_t EtherLevel = 0x63;
		inline constexpr std::size_t Weapon = 0x6A;
		inline constexpr std::size_t Weight = 0x77;
		/** What food has added to the weight. */
		inline constexpr std::size_t WeightGain = 0x78;
		/** Each basic attack's count toward the Deathblows it teaches (7 words). */
		inline constexpr std::size_t DeathblowCounters = 0x90;
		inline constexpr std::size_t Gear = 0xA0;
	}

	namespace Gear
	{
		inline constexpr std::size_t Frame = 0x02;
		inline constexpr std::size_t Engine = 0x03;
		inline constexpr std::size_t Armor = 0x08;
		inline constexpr std::size_t Fuel = 0x38;
		inline constexpr std::size_t MaxFuel = 0x3A;
		inline constexpr std::size_t Hp = 0x60;
		inline constexpr std::size_t MaxHp = 0x64;
		inline constexpr std::size_t BaseMultiplier = 0x74;
	}

	namespace Skills
	{
		inline constexpr std::size_t Deathblows = 0x00;
		inline constexpr std::size_t Abilities = 0x02;
		inline constexpr std::size_t GearDeathblows = 0x04;
		inline constexpr std::size_t GearAbilities = 0x06;
		inline constexpr std::size_t ApPerTurn = 0x17;
		inline constexpr std::size_t Special = 0x1A;
	}

	inline constexpr std::size_t CharacterOffset(std::size_t Character)
	{
		return CharactersOffset + Character * CharacterSize;
	}

	inline constexpr std::size_t GearOffset(std::size_t Gear)
	{
		return GearsOffset + Gear * GearSize;
	}

	inline constexpr std::size_t LearnedSkillsOffsetOf(std::size_t Character)
	{
		return LearnedSkillsOffset + Character * LearnedSkillsSize;
	}
}
