#include "Field/EventVariables.h"

#include "Game/GameState.h"

namespace Weltall
{
	void FEventVariables::EnterField(const FGameState& State)
	{
		for (int32 Index = 0; Index < SavedCount; ++Index)
		{
			Values[static_cast<std::size_t>(Index)] = State.SavedEventVariable(Index);
			Values[static_cast<std::size_t>(SavedCount + Index)] = 0;
		}
	}

	void FEventVariables::Save(FGameState& State) const
	{
		for (int32 Index = 0; Index < SavedCount; ++Index)
		{
			State.SetSavedEventVariable(Index, Values[static_cast<std::size_t>(Index)]);
		}
	}
}
