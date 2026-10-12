#include "Game/PartyOrder.h"

#include <algorithm>
#include <initializer_list>

namespace Weltall
{
	namespace
	{
		constexpr uint8 EmptySlot = 0xFF;
		constexpr uint8 Citan = 2;
		constexpr uint8 Emeralda = 6;
		constexpr uint8 SwordCitan = 9;
		constexpr uint8 GrownEmeralda = 10;
		constexpr int32 NotAPerson = -1;

		/** The person a character is, or NotAPerson for an id past the party's records. */
		int32 PersonOf(uint8 Character)
		{
			if (Character == SwordCitan)
			{
				return Citan;
			}
			if (Character == GrownEmeralda)
			{
				return Emeralda;
			}
			return Character < FPartyOrder::People ? Character : NotAPerson;
		}

		/** The people among Characters, a bit each. */
		uint32 PeopleIn(std::span<const uint8> Characters)
		{
			uint32 People = 0;
			for (const uint8 Character : Characters)
			{
				const int32 Person = PersonOf(Character);
				if (Person != NotAPerson)
				{
					People |= 1u << Person;
				}
			}
			return People;
		}
	}

	bool FPartyOrder::IsFor(const std::array<uint8, Slots>& Party) const
	{
		return IsSet() && PeopleIn(Party) == PeopleIn(std::span<const uint8>(Placed.data(), static_cast<std::size_t>(Count)));
	}

	FPartyOrder::FPlaces FPartyOrder::PlacesOf(const std::array<uint8, Slots>& Party) const
	{
		FPlaces Places{0, 1, 2};
		if (!IsFor(Party))
		{
			return Places;
		}
		// The members by their places in the order, a guest after them; the sort is stable, so guests keep the story's order.
		const auto Rank = [this](uint8 Character)
		{
			const auto End = Placed.begin() + Count;
			const int32 Person = PersonOf(Character);
			return static_cast<int32>((Person == NotAPerson ? End : std::find(Placed.begin(), End, static_cast<uint8>(Person))) - Placed.begin());
		};
		int32 Shown = 0;
		Places.fill(Nobody);
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			if (Party[static_cast<std::size_t>(Slot)] != EmptySlot)
			{
				Places[static_cast<std::size_t>(Shown++)] = static_cast<int8>(Slot);
			}
		}
		std::stable_sort(Places.begin(), Places.begin() + Shown,
			[&Party, &Rank](int8 A, int8 B) { return Rank(Party[static_cast<std::size_t>(A)]) < Rank(Party[static_cast<std::size_t>(B)]); });
		return Places;
	}

	std::array<uint8, FPartyOrder::Slots> FPartyOrder::Shown(const std::array<uint8, Slots>& Party) const
	{
		const FPlaces Places = PlacesOf(Party);
		std::array<uint8, Slots> Result{};
		for (std::size_t Place = 0; Place < Places.size(); ++Place)
		{
			Result[Place] = Places[Place] == Nobody ? EmptySlot : Party[static_cast<std::size_t>(Places[Place])];
		}
		return Result;
	}

	std::array<uint8, FPartyOrder::Slots> FPartyOrder::ShownAsTheyRide(const std::array<uint8, Slots>& Party, const std::array<bool, Slots>& bRides) const
	{
		const FPlaces Places = PlacesOf(Party);
		std::array<uint8, Slots> Result = Party;
		for (const bool bRiding : {true, false})
		{
			// The next place of the order whose member rides alike, for each slot that does.
			std::size_t Next = 0;
			for (std::size_t Slot = 0; Slot < Party.size(); ++Slot)
			{
				if (Party[Slot] == EmptySlot || bRides[Slot] != bRiding)
				{
					continue;
				}
				while (Next < Places.size() && (Places[Next] == Nobody || bRides[static_cast<std::size_t>(Places[Next])] != bRiding))
				{
					++Next;
				}
				if (Next < Places.size())
				{
					Result[Slot] = Party[static_cast<std::size_t>(Places[Next++])];
				}
			}
		}
		return Result;
	}

	void FPartyOrder::Arrange(const std::array<uint8, Slots>& Party, std::span<const uint8> ShownMembers)
	{
		const uint32 Members = PeopleIn(Party);
		// The party's people as the lists name them, each once.
		const auto InOrder = [Members](std::initializer_list<std::span<const uint8>> Lists)
		{
			FPartyOrder Order;
			for (const std::span<const uint8> Characters : Lists)
			{
				for (const uint8 Character : Characters)
				{
					const int32 Person = PersonOf(Character);
					const auto End = Order.Placed.begin() + Order.Count;
					if (Person != NotAPerson && (Members >> Person & 1) != 0 && std::find(Order.Placed.begin(), End, static_cast<uint8>(Person)) == End)
					{
						Order.Placed[static_cast<std::size_t>(Order.Count++)] = static_cast<uint8>(Person);
					}
				}
			}
			return Order;
		};
		const FPartyOrder Arranged = InOrder({ShownMembers, Party});
		*this = Arranged == InOrder({Party}) ? FPartyOrder{} : Arranged;
	}

	void FPartyOrder::Replace(uint8 Leaving, uint8 Joining)
	{
		const int32 Left = PersonOf(Leaving);
		const int32 Joined = PersonOf(Joining);
		const auto End = Placed.begin() + Count;
		const auto Place = std::find(Placed.begin(), End, static_cast<uint8>(Left));
		if (Left == NotAPerson || Joined == NotAPerson || Place == End || std::find(Placed.begin(), End, static_cast<uint8>(Joined)) != End)
		{
			return;
		}
		*Place = static_cast<uint8>(Joined);
	}

	std::array<uint8, FPartyOrder::SavedBytes> FPartyOrder::ToSaved() const
	{
		std::array<uint8, SavedBytes> Bytes{};
		Bytes[0] = static_cast<uint8>(Count);
		std::copy(Placed.begin(), Placed.begin() + Count, Bytes.begin() + 1);
		return Bytes;
	}

	FPartyOrder FPartyOrder::FromSaved(std::span<const uint8, SavedBytes> Bytes)
	{
		// An order is of two people or three: one alone is always in the story's order.
		FPartyOrder Order;
		const int32 Count = Bytes[0];
		if (Count < 2 || Count > Slots)
		{
			return Order;
		}
		uint32 Seen = 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const uint8 Person = Bytes[static_cast<std::size_t>(1 + Index)];
			if (Person >= People || (Seen >> Person & 1) != 0)
			{
				return {};
			}
			Seen |= 1u << Person;
			Order.Placed[static_cast<std::size_t>(Index)] = Person;
		}
		Order.Count = Count;
		return Order;
	}
}
