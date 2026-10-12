#include "Game/GameState.h"
#include "Game/GameStateLayout.h"
#include "Menu/MenuShop.h"

namespace Weltall
{
	namespace
	{
		using namespace GameStateLayout;

		/** The kernel's name tables: weapons, accessories, items. */
		constexpr int32 WeaponNames = 23;
		constexpr int32 AccessoryNames = 17;
		constexpr int32 ItemNames = 22;
		/** A weapon below this id is held in the weapon's slot; above, it is Billy's ammunition. */
		constexpr uint8 FirstAmmunition = 0x32;
		constexpr std::size_t WeaponSlots = 0x6A;
		constexpr std::size_t GunSlots = 0x6F;
		constexpr std::size_t AccessorySlots = 0x74;
	}

	void FMenuShop::DrawGold(uint32 OnEntry, uint32 Total, uint32 Left)
	{
		const FMenuShopTables& Tables = Resources.Shop();
		const std::array<uint32, 3> Values = {OnEntry, Total, Left};
		for (std::size_t Line = 0; Line < Values.size(); ++Line)
		{
			DrawDigitsInto(Scene.Gold[Line], Values[Line], 0, 9, Tables.GoldX[Line], Tables.GoldY[Line], false);
		}
		Scene.bGoldShown = true;
	}

	uint16 FMenuShop::WornMask(uint8 Id, EShopKind Kind) const
	{
		// Which available characters wear the goods: a weapon in the weapon's slot and the four bytes after
		// it, ammunition in the guns' and the byte after them; an accessory in its three slots.
		if (Id == 0 || Kind == EShopKind::Item)
		{
			return 0;
		}
		const std::span<const uint8> Block = Services.Game->Block();
		uint16 Mask = 0;
		for (int32 Character = 0; Character < static_cast<int32>(Available.size()); ++Character)
		{
			if (!Available[static_cast<std::size_t>(Character)])
			{
				continue;
			}
			const std::size_t Record = CharacterOffset(static_cast<std::size_t>(Character));
			const std::size_t First = Record + (Kind == EShopKind::Accessory ? AccessorySlots : (Id < FirstAmmunition ? WeaponSlots : GunSlots));
			const std::size_t Slots = Kind == EShopKind::Accessory ? 3 : 5;
			for (std::size_t Slot = 0; Slot < Slots; ++Slot)
			{
				if (First + Slot < Block.size() && Block[First + Slot] == Id)
				{
					Mask = static_cast<uint16>(Mask | 1u << Character);
					break;
				}
			}
		}
		return Mask;
	}

	std::span<const uint8> FMenuShop::GoodsName(uint8 Id, EShopKind Kind) const
	{
		const int32 Names = Kind == EShopKind::Accessory ? AccessoryNames : (Kind == EShopKind::Item ? ItemNames : WeaponNames);
		return Services.Text.Tables != nullptr ? Services.Text.Tables->String(Names, Id) : std::span<const uint8>{};
	}

	std::span<const uint8> FMenuShop::GoodsRecord(uint8 Id, EShopKind Kind) const
	{
		if (Kind == EShopKind::Item)
		{
			const std::span<const uint8> Items = Resources.Items();
			const std::size_t At = static_cast<std::size_t>(Id) * 0x10;
			return At + 0x10 <= Items.size() ? Items.subspan(At, 0x10) : std::span<const uint8>{};
		}
		return Kind == EShopKind::Accessory ? Resources.Equipment().Accessory(Id) : Resources.Equipment().Weapon(Id);
	}

	void FMenuShop::CloseList(bool bCloses, bool bBuying)
	{
		ClearScene();
		if (bCloses)
		{
			CloseWindow(2);
			CloseWindow(3);
			if (bBuying)
			{
				CloseWindow(5);
			}
			bScrollThumbShown = false;
			bListCursorShown = false;
		}
	}
}
