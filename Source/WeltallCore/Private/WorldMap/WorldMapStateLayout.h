#pragma once

#include "Foundation/Types.h"

#include <cstddef>

namespace Weltall::WorldMapStateLayout
{
	/** The party's place on foot (whole units) and the headings: the leader's, then each slot's gear's. */
	inline constexpr std::size_t PartyX = 0x1820;
	inline constexpr std::size_t PartyZ = 0x1822;
	inline constexpr std::size_t LeaderHeading = 0x1824;
	inline constexpr std::size_t GearHeading(std::size_t Slot) { return 0x1826 + 2 * Slot; }
	/** The Yggdrasil's place (whole units), heading and flags. */
	inline constexpr std::size_t VehicleX = 0x182C;
	inline constexpr std::size_t VehicleY = 0x182E;
	inline constexpr std::size_t VehicleZ = 0x1830;
	inline constexpr std::size_t VehicleHeading = 0x1832;
	inline constexpr std::size_t VehicleFlags = 0x1834;
	/** Set while the menu runs, so a save made there starts the world map where the party stood. */
	inline constexpr std::size_t MenuOpen = 0x1836;
	/** Each slot's riding flag as handed to a battle (a word each). */
	inline constexpr std::size_t BattleRiding = 0x183C;
	inline constexpr std::size_t MinimapHidden = 0x1842;
	/** Thames's place (whole units), its waypoint, and whether they are kept. */
	inline constexpr std::size_t ThamesX = 0x1844;
	inline constexpr std::size_t ThamesZ = 0x1846;
	inline constexpr std::size_t ThamesWaypoint = 0x1848;
	inline constexpr std::size_t ThamesSaved = 0x184A;
	/** Shevat's place: each coordinate's low half (20.12) and whole units; whether they are kept. */
	inline constexpr std::size_t ShevatXLow = 0x184C;
	inline constexpr std::size_t ShevatX = 0x184E;
	inline constexpr std::size_t ShevatZLow = 0x1850;
	inline constexpr std::size_t ShevatZ = 0x1852;
	inline constexpr std::size_t ShevatSaved = 0x1854;
	/** The saved event variables 0 (the story), 1 and 2 (the world-map location left from). */
	inline constexpr std::size_t StoryVariable = 0x1930;
	inline constexpr std::size_t SecondVariable = 0x1932;
	inline constexpr std::size_t LocationVariable = 0x1934;
	/** Each slot's gear: flags (400h and up: on the map), x and z (whole units). */
	inline constexpr std::size_t GearFlags(std::size_t Slot) { return 0x195A + 6 * Slot; }
	inline constexpr std::size_t GearX(std::size_t Slot) { return 0x195C + 6 * Slot; }
	inline constexpr std::size_t GearZ(std::size_t Slot) { return 0x195E + 6 * Slot; }
	/** The places the minimap shows, a bit each. */
	inline constexpr std::size_t MinimapDots = 0x1B2C;
	inline constexpr std::size_t Party = 0x1D34;
	inline constexpr std::size_t Riding = 0x22B1;
	inline constexpr std::size_t CharacterGear(std::size_t Character) { return 0x26C + 0xA4 * Character + 0xA0; }
	inline constexpr std::size_t Place = 0x231A;
	inline constexpr std::size_t CameraYaw = 0x231C;
	inline constexpr std::size_t Entrance = 0x231E;
	inline constexpr std::size_t Scene = 0x2320;

	/** The vehicle flags: the type (0 none), start at the vehicle, aboard. */
	inline constexpr uint16 VehicleTypeMask = 0x1FFF;
	inline constexpr uint16 StartAtVehicle = 0x2000;
	inline constexpr uint16 Aboard = 0x4000;
	inline constexpr int16 GearOnMap = 0x400;
	/** The scene word's flag for a return from a battle. */
	inline constexpr uint16 FromBattle = 0x8000;
}
