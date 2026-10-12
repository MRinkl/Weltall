#include "Game/GameState.h"
#include "Game/GameStateLayout.h"
#include "Menu/MenuMain.h"

namespace Weltall
{
	namespace
	{
		constexpr uint8 Billy = 4;
		constexpr uint8 FullLoad = 100;
		constexpr uint8 MostHeld = 99;

		/** The record's equipment: the weapon's five bytes, the guns' five, the accessories' five; a gear's four of each. */
		namespace CharacterSlots
		{
			constexpr std::size_t Weapon = 0x6A;
			constexpr std::size_t Guns = 0x6F;
			constexpr std::size_t Accessories = 0x74;
		}
		namespace GearSlots
		{
			constexpr std::size_t Weapon = 0x0C;
			constexpr std::size_t Guns = 0x04;
			constexpr std::size_t Parts = 0x09;
		}
		/** Where the slots are kept aside: the weapon's block, the guns', the accessories'. */
		constexpr std::size_t SavedWeapon = 0;
		constexpr std::size_t SavedGuns = 5;
		constexpr std::size_t SavedAccessories = 10;

		/** An inventory list: its ids and counts, and how many slots it has. */
		struct FList
		{
			std::span<uint8> Items;
			std::span<uint8> Counts;
		};

		FList InventoryList(FGameState& Game, int32 List)
		{
			return {Game.InventoryItems(List), Game.InventoryCounts(List)};
		}

		/** The equipment's inventory lists, numbered as the scripts number them. */
		constexpr int32 WeaponList = 1;
		constexpr int32 AccessoryList = 2;
		constexpr int32 GearWeaponList = 3;
		constexpr int32 GearPartList = 4;

		/** The worn item goes to the inventory, the new one comes out of it; then every count is kept to 99 and every empty slot cleared. */
		void Exchange(FList List, uint8 New, uint8 Old)
		{
			if (New != 0)
			{
				for (std::size_t At = 0; At < List.Items.size(); ++At)
				{
					if (List.Items[At] == New)
					{
						--List.Counts[At];
						break;
					}
				}
			}
			if (Old != 0)
			{
				bool bFound = false;
				for (std::size_t At = 0; At < List.Items.size(); ++At)
				{
					if (List.Items[At] == Old)
					{
						++List.Counts[At];
						bFound = true;
						break;
					}
				}
				for (std::size_t At = 0; !bFound && At < List.Items.size(); ++At)
				{
					if (List.Items[At] == 0)
					{
						List.Items[At] = Old;
						List.Counts[At] = 1;
						break;
					}
				}
			}
			for (std::size_t At = 0; At < List.Items.size(); ++At)
			{
				if (List.Counts[At] == 0)
				{
					List.Items[At] = 0;
				}
				else if (List.Counts[At] > MostHeld)
				{
					List.Counts[At] = MostHeld;
				}
			}
		}
	}

	void FMenuMain::SaveEquip(int32 Slot, bool bGear)
	{
		EquipList.SavedStats = ShownStats;
		const std::size_t Bytes = bGear ? 4 : 5;
		for (std::size_t At = 0; At < Bytes; ++At)
		{
			EquipList.SavedSlots[SavedWeapon + At] = RecordByte(Slot, bGear, (bGear ? GearSlots::Weapon : CharacterSlots::Weapon) + At);
			EquipList.SavedSlots[SavedGuns + At] = RecordByte(Slot, bGear, (bGear ? GearSlots::Guns : CharacterSlots::Guns) + At);
			EquipList.SavedSlots[SavedAccessories + At] = RecordByte(Slot, bGear, (bGear ? GearSlots::Parts : CharacterSlots::Accessories) + At);
		}
	}

	void FMenuMain::RevertEquip(int32 Slot, bool bGear)
	{
		const std::size_t Bytes = bGear ? 4 : 5;
		for (std::size_t At = 0; At < Bytes; ++At)
		{
			RecordByte(Slot, bGear, (bGear ? GearSlots::Weapon : CharacterSlots::Weapon) + At) = EquipList.SavedSlots[SavedWeapon + At];
			RecordByte(Slot, bGear, (bGear ? GearSlots::Guns : CharacterSlots::Guns) + At) = EquipList.SavedSlots[SavedGuns + At];
		}
		for (std::size_t At = 0; At < 3; ++At)
		{
			RecordByte(Slot, bGear, (bGear ? GearSlots::Parts : CharacterSlots::Accessories) + At) = EquipList.SavedSlots[SavedAccessories + At];
		}
	}

	void FMenuMain::PreviewEquip(int32 Slot, int32 Which, int32 Row, int32 Top, int32 Page, bool bGear)
	{
		const uint8 Candidate = EquipList.Candidates[static_cast<std::size_t>(Top + Row)];
		const std::size_t At = static_cast<std::size_t>(Which);
		if (Page != 0)
		{
			RecordByte(Slot, bGear, (bGear ? GearSlots::Guns : CharacterSlots::Guns) + At) = Candidate;
		}
		else if (Which == 0)
		{
			RecordByte(Slot, bGear, bGear ? GearSlots::Weapon : CharacterSlots::Weapon) = Candidate;
		}
		else if (Which > 0 && Which <= 3)
		{
			RecordByte(Slot, bGear, (bGear ? GearSlots::Parts : CharacterSlots::Accessories) + At - 1) = Candidate;
		}
	}

	bool FMenuMain::CommitEquip(int32 Slot, int32 Which, int32 Page, bool bGear)
	{
		FGameState& Game = *Services.Game;
		const std::size_t At = static_cast<std::size_t>(Which);
		FList List = InventoryList(Game, bGear ? GearWeaponList : WeaponList);
		bool bExchanges = true;
		bool bBillysWeapon = false;
		uint8 New = 0;
		uint8 Old = 0;
		if (Page == 0)
		{
			if (Which == 0)
			{
				uint8& Weapon = RecordByte(Slot, bGear, bGear ? GearSlots::Weapon : CharacterSlots::Weapon);
				New = Weapon;
				Old = EquipList.SavedSlots[SavedWeapon];
				if (New == 0)
				{
					Weapon = Old;
					bExchanges = false;
				}
				else
				{
					bBillysWeapon = Member(Slot) == Billy;
				}
			}
			else
			{
				List = InventoryList(Game, bGear ? GearPartList : AccessoryList);
				Old = EquipList.SavedSlots[SavedAccessories + At - 1];
				New = RecordByte(Slot, bGear, (bGear ? GearSlots::Parts : CharacterSlots::Accessories) + At - 1);
			}
		}
		else
		{
			uint8& Loaded = RecordByte(Slot, bGear, (bGear ? GearSlots::Guns : CharacterSlots::Guns) + At);
			New = Loaded;
			Old = EquipList.SavedSlots[SavedGuns + At];
			if (New == 0)
			{
				Loaded = Old;
				bExchanges = false;
			}
			else if (!Services.Options.bAmmunitionKept)
			{
				// Billy's ammunition: a new load is full; the one it replaces is returned only while the new
				// type's count is full, so a partly spent load, and often a full one, is lost.
				uint8& Count = AmmunitionCount(bGear, New);
				if (Count < FullLoad)
				{
					Old = 0;
				}
				Count = FullLoad;
			}
			else
			{
				KeepAmmunition(Slot, bGear, At, New, Old);
				bExchanges = false;
			}
		}
		if (bExchanges)
		{
			Exchange(List, New, Old);
		}
		return bBillysWeapon;
	}

	uint8& FMenuMain::AmmunitionCount(bool bGear, uint8 Ammunition)
	{
		const std::size_t Counts = bGear ? GameStateLayout::StoryFlagsOffset : GameStateLayout::AmmunitionCountsOffset;
		return Services.Game->Block()[Counts + Ammunition];
	}

	bool FMenuMain::OtherGunHolds(int32 Slot, bool bGear, std::size_t At, uint8 Ammunition)
	{
		for (std::size_t Gun = 0; Gun < 4; ++Gun)
		{
			if (Gun != At && RecordByte(Slot, bGear, (bGear ? GearSlots::Guns : CharacterSlots::Guns) + Gun) == Ammunition)
			{
				return true;
			}
		}
		return false;
	}

	void FMenuMain::KeepAmmunition(int32 Slot, bool bGear, std::size_t At, uint8 New, uint8 Old)
	{
		// A type's count is the shots of its one open load. The load replaced goes back while it has shots
		// and no other gun uses it; the new one is a fresh load only when its type has none left, else the
		// open load: already in this gun or the other, or the one waiting in the inventory.
		const uint8 Returned = Old != 0 && Old != New && AmmunitionCount(bGear, Old) > 0 && !OtherGunHolds(Slot, bGear, At, Old) ? Old : 0;
		uint8 Taken = New;
		uint8& Count = AmmunitionCount(bGear, New);
		if (Count == 0)
		{
			Count = FullLoad;
		}
		else if (New == Old || OtherGunHolds(Slot, bGear, At, New))
		{
			Taken = 0;
		}
		Exchange(InventoryList(*Services.Game, bGear ? GearWeaponList : WeaponList), Taken, Returned);
	}

	void FMenuMain::ReturnBillyAmmo(int32 Slot, bool bGear)
	{
		uint8& Loaded = RecordByte(Slot, bGear, bGear ? GearSlots::Guns : CharacterSlots::Guns);
		const uint8 Ammunition = Loaded;
		Loaded = 0;
		FList List = InventoryList(*Services.Game, bGear ? GearWeaponList : WeaponList);
		// Every slot holding the ammunition gains one, even the empty slots when nothing was loaded.
		bool bFound = false;
		for (std::size_t At = 0; At < List.Items.size(); ++At)
		{
			if (List.Items[At] == Ammunition)
			{
				const uint8 Count = static_cast<uint8>(List.Counts[At] + 1);
				List.Counts[At] = Count > MostHeld ? MostHeld : Count;
				bFound = true;
			}
		}
		for (std::size_t At = 0; !bFound && At < List.Items.size(); ++At)
		{
			if (List.Items[At] == 0)
			{
				List.Items[At] = Ammunition;
				List.Counts[At] = 1;
				return;
			}
		}
	}
}
