#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Game/PartyOrder.h"
#include "Game/StoryMode.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	struct FCardSave;

	/**
	 * A save of the game as the console makes it: the game's data from its memory-card block (after the
	 * card's title and icon), which the port keeps byte for byte and loads as the Menu loads it. What the
	 * port keeps of its own lies in bytes of it the game writes as zeros and never reads.
	 */
	class FOriginalSave
	{
	public:
		static constexpr std::size_t DataOffset = 0x100;
		static constexpr std::size_t DataSize = 0x1F00;
		static constexpr uint8 EmptySlot = 0xFF;

		/**
		 * The game's save from a card: its file name must be the game's, and its checksum must hold,
		 * as the Menu requires before it loads one.
		 */
		WELTALLCORE_API static TResult<FOriginalSave> FromCard(const FCardSave& Save);

		/** A save's game data, refused unless it is DataSize bytes and its checksum holds. */
		WELTALLCORE_API static TResult<FOriginalSave> FromData(std::span<const uint8> Data);

		/** Game data of DataSize bytes as the game writes it: its last byte set to the checksum of the others. */
		WELTALLCORE_API static FOriginalSave WithChecksum(std::vector<uint8> Data);

		/** The data as the game reads it. */
		std::span<const uint8> Data() const { return Bytes; }

		/**
		 * The data as the port plays it: Data, but for a save of the expanded second disc made where the original
		 * stands elsewhere, whose own place and story are put back over the original's (SaveWrite.h).
		 */
		WELTALLCORE_API std::vector<uint8> PlayedData() const;

		/** The play clock, in vertical blanks. */
		WELTALLCORE_API uint32 PlayTime() const;

		/** The story's progress (event variable 0) where the port plays the save, which picks the chapter's title. */
		WELTALLCORE_API uint16 StoryProgress() const;

		/** The party's three character ids, EmptySlot where nobody is. */
		WELTALLCORE_API std::array<uint8, 3> Party() const;

		/** The party's levels, slot by slot. */
		WELTALLCORE_API std::array<uint8, 3> Levels() const;

		/** The card slot (0-14) the game saved it to. */
		WELTALLCORE_API int32 CardSlot() const;

		/** How its game tells the second disc's story, which only the port's saves keep: none chosen in any other. */
		WELTALLCORE_API EStoryMode StoryMode() const;

		/** The order its player kept the party in, which only the port's saves keep: none set in any other. */
		WELTALLCORE_API FPartyOrder PartyOrder() const;

	private:
		/** Whether the port kept anything of its own in it. */
		bool HasPortPart() const;

		std::vector<uint8> Bytes;
	};
}
