#pragma once

#include "Foundation/Types.h"

#include <span>

namespace Weltall
{
	class FGameState;
	class FGearPartTables;

	/**
	 * The fields of a gear record that the Menu derives from the gear's parts, recomputed as the Menu
	 * does after loading a save and after every change of part. Each returns false, changing nothing,
	 * when the gear names a record the tables lack.
	 */

	/** From the frame: HP and maximum HP (the gear is fully repaired), agility, ether amplifier and hit bonus. */
	[[nodiscard]] WELTALLCORE_API bool ApplyGearFrame(FGameState& State, int32 Gear, const FGearPartTables& Tables);

	/**
	 * From the frame as the gear shop fits one: the maximum HP (HP only capped to it, not repaired), the
	 * frame's weight, agility, ether amplifier and hit bonus.
	 */
	[[nodiscard]] WELTALLCORE_API bool RefitGearFrame(FGameState& State, int32 Gear, const FGearPartTables& Tables);

	/** From the armor: defense and ether defense. */
	[[nodiscard]] WELTALLCORE_API bool ApplyGearArmor(FGameState& State, int32 Gear, const FGearPartTables& Tables);

	/** From the engine: maximum fuel (the fuel is capped to it) and output. */
	[[nodiscard]] WELTALLCORE_API bool ApplyGearEngine(FGameState& State, int32 Gear, const FGearPartTables& Tables);

	/**
	 * From the three parts: their sums (defense, ether defense, weight, guard, hit, agility per
	 * terrain, ether amplifier), their specials and the overweight penalty; the specials also set the
	 * pilot's gear Deathblows and repair ability.
	 */
	[[nodiscard]] WELTALLCORE_API bool ApplyGearParts(FGameState& State, int32 Gear, const FGearPartTables& Tables);

	/** Whose copy of the parts' recompute runs: the Menu's own, or the gear shop's. */
	enum class EGearPartsCopy : uint8
	{
		Menu,
		/** Leaves the parts' ether amplifier out of the gear, and clears other Deathblow bits before the specials grant theirs. */
		GearShop,
	};

	/** From the three parts as above, for the pilot given, by the copy given; false for a pilot without records. */
	[[nodiscard]] WELTALLCORE_API bool ApplyGearParts(FGameState& State, int32 Gear, const FGearPartTables& Tables, uint8 Pilot, EGearPartsCopy Copy);

	/**
	 * From the gear weapon (a gear weapons' table of 0x14-byte records): its attack, effect, chance and
	 * kind in the gear's weapon slot (an elemental weapon's effect in its element word), and for Billy's
	 * gears each gun slot's own weapon.
	 */
	[[nodiscard]] WELTALLCORE_API bool ApplyGearWeapon(FGameState& State, int32 Gear, std::span<const uint8> GearWeapons);
}
