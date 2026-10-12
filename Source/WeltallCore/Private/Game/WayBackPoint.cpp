#include "Game/WayBackPoint.h"

#include "Game/GameState.h"

namespace Weltall
{
	namespace
	{
		/** The story's progress, and the entrance a field map is entered by (event variables 0 and 1: bytes 000h and 002h). */
		constexpr int32 StoryVariable = 0;
		constexpr int32 EntranceVariable = 1;
	}

	void FWayBackPoint::Apply(FGameState& State) const
	{
		State.SetPlace(Map);
		State.SetFieldEntrance(Entrance);
		State.SetSavedEventVariable(EntranceVariable, Entrance);
		State.SetSavedEventVariable(StoryVariable, Story);
	}
}
