#pragma once

#include "Foundation/Types.h"

#include <array>
#include <span>

namespace Weltall
{
	/**
	 * The order the player keeps the party in, the port's own. The game's party keeps the order the story gives it,
	 * which its scenes and staged battles are written for; the player's order is what the party shows where that order
	 * does not matter to the story: walking, the menus' panels, the battles no event stages.
	 *
	 * Kept for the people it was arranged with: while the story has anyone else in the party, or one of them out of it,
	 * the party shows the story's order, and the same people together again show the player's. The story's later
	 * Citan and Emeralda count as the earlier; a guest no menu shows is none of the people and is shown last.
	 */
	class FPartyOrder
	{
	public:
		static constexpr int32 Slots = 3;
		/** Fei to Maria; the story's later records of Citan and Emeralda count as them. */
		static constexpr int32 People = 9;
		static constexpr int8 Nobody = -1;
		/** A save keeps the count of people placed, then them. */
		static constexpr std::size_t SavedBytes = 1 + Slots;

		/** For each place the party is shown in, the leader's first, the party slot shown there; Nobody past its members. */
		using FPlaces = std::array<int8, Slots>;

		/** Whether the player keeps an order: until then, or once it is cleared, the party shows the story's. */
		bool IsSet() const { return Count != 0; }

		/** Whether Party (a character per slot, 0xFF where nobody is) is the people the order was arranged with, and so shows it. */
		WELTALLCORE_API bool IsFor(const std::array<uint8, Slots>& Party) const;

		/** Where Party is shown: the story's slots as they stand unless the order IsFor it. */
		WELTALLCORE_API FPlaces PlacesOf(const std::array<uint8, Slots>& Party) const;

		/** Party's characters in the places they are shown in, 0xFF past its members. */
		WELTALLCORE_API std::array<uint8, Slots> Shown(const std::array<uint8, Slots>& Party) const;

		/**
		 * The member each of the party's slots shows where some ride their gears and some walk: a member is shown in
		 * a slot that rides or walks as its own does, those riding taking the riding slots in their player's order
		 * and those on foot the others in theirs. An empty slot stays empty.
		 */
		WELTALLCORE_API std::array<uint8, Slots> ShownAsTheyRide(const std::array<uint8, Slots>& Party, const std::array<bool, Slots>& bRides) const;

		/**
		 * The player shows Party as Shown (characters, 0xFF past them): the order is Party's people's from now on, any
		 * member Shown leaves out after the others in the story's order. Shown in the story's order keeps no order.
		 */
		WELTALLCORE_API void Arrange(const std::array<uint8, Slots>& Party, std::span<const uint8> Shown);

		/** A member of the people arranged leaves the party for another (Member Change): the newcomer takes its place. */
		WELTALLCORE_API void Replace(uint8 Leaving, uint8 Joining);

		/** As a save keeps it; FromSaved reads it back, unset from bytes that hold no order of a party. */
		WELTALLCORE_API std::array<uint8, SavedBytes> ToSaved() const;
		WELTALLCORE_API static FPartyOrder FromSaved(std::span<const uint8, SavedBytes> Bytes);

		bool operator==(const FPartyOrder&) const = default;

	private:
		/** The people arranged, the leader first. */
		std::array<uint8, Slots> Placed{};
		int32 Count = 0;
	};
}
