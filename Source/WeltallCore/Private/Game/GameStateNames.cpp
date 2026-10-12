#include "Game/GameStateNames.h"

#include "Game/GameState.h"
#include "Game/GameStateLayout.h"

namespace Weltall
{
	namespace
	{
		/** The party codes start here: 80h names the member in the party's first slot. */
		constexpr int32 FirstPartyCode = 0x80;
	}

	std::span<const uint8> FGameStateNames::Name(int32 Number) const
	{
		const std::span<const uint8> Block = State.Block();
		const std::size_t Start = static_cast<std::size_t>(Number) * FGameState::NameBytes;
		return Number >= 0 && Start < Block.size() ? Block.subspan(Start) : std::span<const uint8>();
	}

	uint8 FGameStateNames::PartyName(int32 Code) const
	{
		const std::size_t At = GameStateLayout::PartyOffset + static_cast<std::size_t>(Code - FirstPartyCode);
		return Code >= FirstPartyCode && At < FGameState::BlockSize ? State.Block()[At] : FGameState::EmptySlot;
	}
}
