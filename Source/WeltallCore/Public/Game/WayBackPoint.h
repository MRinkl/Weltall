#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	class FGameState;

	/**
	 * A point of the original's second disc that a game of the expanded one goes on from there, its saves made as the
	 * original's: a field map, the entrance it is entered by, and the story's progress there.
	 */
	struct FWayBackPoint
	{
		uint16 Map = 0;
		uint16 Entrance = 0;
		uint16 Story = 0;

		/** State put there: entering the map by its entrance, as the field takes a loaded game there, the story at its progress. */
		WELTALLCORE_API void Apply(FGameState& State) const;
	};
}
