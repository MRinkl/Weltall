#pragma once

#include "Foundation/Types.h"

#include <vector>

namespace Weltall
{
	class FGameState;
	struct FWorldMapContext;
}

namespace Weltall::Testing
{
	/** A place of the world map the party's travel reaches and may enter. */
	struct FWorldMapWay
	{
		/** Its record: the list (0 on foot, 1 in gears, 2 aboard the ship) and its number there. */
		int32 List = 0;
		int32 Record = 0;
		/** Where the party stands in it, whole units. */
		int32 X = 0;
		int32 Z = 0;
		/** The party leaves its gears first: a place entered on foot, come to in gears. */
		bool bOnFoot = false;
	};

	/**
	 * The places the party can travel to from where it stands on the world map and enter, each field map and entry
	 * once: the records of its travel's list (on foot, in gears, aboard) whose rectangle holds ground that its
	 * travel reaches over terrain it may cross, the world wrapping at its edges. A party in gears also reaches
	 * what it walks to once it has left them. A strip that takes the party in as it walks onto it ends a way there.
	 */
	std::vector<FWorldMapWay> WorldMapWays(const FWorldMapContext& Context);

	/**
	 * Whether the world map moves the party by itself: the ship on one of its own voyages (to the hideout, the
	 * factory, Nisan), under the sand of the Road to Nisan, or rising, sinking, climbing or landing. The travel is
	 * the player's only once it is over.
	 */
	bool IsWorldMapVoyage(const FWorldMapContext& Context);

	/**
	 * Leaves in State where the party's ship stands as the world map runs, and the word that has the world map
	 * start there: a state kept aboard then goes on where it was. The game keeps those words only as the ship is
	 * left for its bridge, and a state taken while it sails would start the world map at the entrance the party
	 * last came onto it by. On foot or in gears nothing is changed: there the entrance is where the party stands.
	 */
	void KeepShipOnWorldMap(FGameState& State, const FWorldMapContext& Context);

	/**
	 * Has the world map start from State with the party standing in a way's place, as a game saved there on the
	 * world map and loaded starts.
	 */
	void StandOnWorldMap(FGameState& State, const FWorldMapWay& Way);
}
