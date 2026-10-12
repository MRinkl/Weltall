#pragma once

#include "Text/MesWindow.h"

namespace Weltall
{
	class FGameState;

	/**
	 * The names a message's inserts read from the game's state, as the kernel reads them from the
	 * state block: a name runs to its terminator, even past its slot, and a party code reads the
	 * byte that far into the party.
	 */
	class FGameStateNames final : public IMesGameState
	{
	public:
		explicit FGameStateNames(const FGameState& InState) : State(InState) {}

		WELTALLCORE_API std::span<const uint8> Name(int32 Number) const override;
		WELTALLCORE_API uint8 PartyName(int32 Code) const override;

	private:
		const FGameState& State;
	};
}
