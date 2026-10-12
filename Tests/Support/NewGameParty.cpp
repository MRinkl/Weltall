#include "Support/NewGameParty.h"

#include "Media/GameFileIndex.h"

#include <algorithm>

namespace Weltall::Testing
{
	namespace
	{
		constexpr int32 NewGameDirectory = 16;
		constexpr int32 NewGameFile = 3;
		constexpr std::size_t JoinedOffset = 0x1D30;
		constexpr std::size_t JoinedBytes = 2;
		constexpr std::size_t PartyOffset = 0x1D34;
		constexpr std::size_t PartyBytes = 3;
		constexpr std::size_t InGearOffset = 0x22B1;
		constexpr std::size_t InGearBytes = 3;
		constexpr std::size_t GearOffset = 0x26C + 0xA0;
		constexpr std::size_t CharacterSize = 0xA4;
	}

	bool CopyNewGameParty(const FCdImage& Image, const FGameFileIndex& Index, FGameState& State)
	{
		const FIndexRecord* Record = Index.Find(NewGameDirectory, NewGameFile);
		if (Record == nullptr)
		{
			return false;
		}
		const auto File = ReadFileSectors(Image, *Record);
		if (!File || File->size() < FGameState::BlockSize)
		{
			return false;
		}
		const auto Copy = [&File, &State](std::size_t Offset, std::size_t Count) {
			std::copy_n(File->begin() + static_cast<std::ptrdiff_t>(Offset), Count, State.Block().begin() + static_cast<std::ptrdiff_t>(Offset));
		};
		Copy(JoinedOffset, JoinedBytes);
		Copy(PartyOffset, PartyBytes);
		Copy(InGearOffset, InGearBytes);
		for (int32 Character = 0; Character < FGameState::CharacterCount; ++Character)
		{
			Copy(GearOffset + CharacterSize * static_cast<std::size_t>(Character), 1);
		}
		return true;
	}
}
