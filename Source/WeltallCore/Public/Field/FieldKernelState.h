#pragma once

#include "Field/EventContext.h"
#include "Field/FieldReturn.h"
#include "Graphics/SpriteSystem.h"

#include <array>
#include <optional>

namespace Weltall
{
	/**
	 * The kernel's globals the field reads and writes, which outlive the Field program: kept from one of
	 * its runs to the next (across a battle, the world map or another program) as a map change keeps
	 * them from map to map. As the console powers on: nothing drawn, the field clock off, no menu asked
	 * for, the sprite ring at its start.
	 */
	struct FFieldKernelState
	{
		/** The place's formations' weights, which a map without encounter data keeps. */
		std::array<uint8, FFieldEncounters::FormationCount> EncounterWeights{};
		/** The battle's formation: the last one an encounter drew or a script named. */
		uint8 BattleFormation = 0;
		/** The gears can be neither boarded nor left: as the field or the world map last worked it out. */
		bool bGearsLocked = false;
		/** The arena's result of its last match, which the field's scripts read. */
		uint8 BattlingResult = 0;
		FFieldClock Clock;
		/** The parameter the field hands the menu, and the menus asked for since the last one closed. */
		uint8 MenuParam = 0;
		int32 MenusPending = 0;
		/** Where the kernel's sprite ring puts the next streamed frame. */
		FSpriteSystem::FRingPlace SpriteRingPlace;
		/**
		 * The state the field saved as it last left for a battle (unless its scripts named the map after
		 * it) or the arena, which it comes back to when it starts again with the trips counted.
		 */
		std::optional<FFieldSnapshot> Saved;
	};
}
