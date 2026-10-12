#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Game/GameStateLayout.h"
#include "Game/GearRecompute.h"
#include "Menu/MenuGearShop.h"

namespace Weltall
{
	namespace
	{
		using namespace GameStateLayout;
		using LittleEndian::ReadU16;
		using LittleEndian::ReadU32;

		/** The kernel's name tables: gear parts, gear weapons. */
		constexpr int32 PartNames = 50;
		constexpr int32 WeaponNames = 51;
		constexpr std::size_t WeaponSize = 0x14;
		/** Where each kind's record keeps its price, and which gears may fit it (a bit by gear). */
		constexpr std::array<std::size_t, 5> PriceFields = {0x04, 0x0A, 0x0A, 0x04, 0x08};
		constexpr std::array<std::size_t, 5> UsersFields = {0x00, 0x00, 0x00, 0x00, 0x04};
		/** A gear's fitted armor, frame, engine and parts; its weapon slots and Billy's guns'. */
		constexpr std::size_t ArmorSlot = 0x08;
		constexpr std::size_t FrameSlot = 0x02;
		constexpr std::size_t EngineSlot = 0x03;
		constexpr std::size_t PartSlots = 0x09;
		constexpr std::size_t WeaponSlots = 0x0C;
		constexpr std::size_t GunSlots = 0x04;
		/** A gear weapon below this id is looked for in the weapon's slots, from it on in the guns'. */
		constexpr uint8 FirstGun = 0x32;
		/** The fields the shop compares: armor's and parts' defense, the weapon's attack (and a second gun's), engine output, attack multipliers. */
		constexpr std::size_t ArmorDefense = 0x70;
		constexpr std::size_t PartsDefense = 0x40;
		constexpr std::size_t WeaponAttack = 0x12;
		constexpr std::size_t SecondGunAttack = 0x22;
		constexpr std::size_t Output = 0x3C;
		constexpr std::size_t AttackMultiplier = 0x74;
		constexpr std::size_t AttackMultiplierBonus = 0x56;
		/** Billy's gears, whose attack is six tenths of two guns'. */
		constexpr std::array<uint8, 2> GunGears = {5, 13};
		/** The gear whose pilot the story changes, the flag that does, and the pilot it takes. */
		constexpr std::size_t GrownEmeraldaGear = 9;
		constexpr std::size_t StoryFlags = 0x22B6;
		constexpr uint16 GrownEmeraldaFlag = 0x1000;
		constexpr uint8 GrownEmeralda = 10;

		std::span<const uint8> Row(std::span<const uint8> Table, uint8 Id, std::size_t Size)
		{
			const std::size_t At = static_cast<std::size_t>(Id) * Size;
			return At + Size <= Table.size() ? Table.subspan(At, Size) : std::span<const uint8>{};
		}
	}

	std::span<const uint8> FMenuGearShop::GoodsRecord(uint8 Id, EGearGoods Kind) const
	{
		const FGearPartTables& Parts = Resources.GearParts();
		switch (Kind)
		{
		case EGearGoods::Armor:
			return Parts.Armor(Id);
		case EGearGoods::Frame:
			return Parts.Frame(Id);
		case EGearGoods::Engine:
			return Parts.Engine(Id);
		case EGearGoods::Part:
			return Parts.Part(Id);
		case EGearGoods::Weapon:
			return Row(Resources.GearWeapons(), Id, WeaponSize);
		}
		return {};
	}

	std::span<const uint8> FMenuGearShop::GoodsName(uint8 Id, EGearGoods Kind) const
	{
		switch (Kind)
		{
		case EGearGoods::Armor:
			return Resources.Text(FMenuResources::EText::GearArmorNames).String(Id);
		case EGearGoods::Frame:
			return Resources.Text(FMenuResources::EText::GearFrameNames).String(Id);
		case EGearGoods::Engine:
			return Resources.Text(FMenuResources::EText::GearEngineNames).String(Id);
		case EGearGoods::Part:
		case EGearGoods::Weapon:
			return Services.Text.Tables != nullptr ? Services.Text.Tables->String(Kind == EGearGoods::Part ? PartNames : WeaponNames, Id) : std::span<const uint8>{};
		}
		return {};
	}

	uint32 FMenuGearShop::GoodsPrice(uint8 Id, EGearGoods Kind) const
	{
		const std::span<const uint8> Record = GoodsRecord(Id, Kind);
		const std::size_t At = PriceFields[static_cast<std::size_t>(Kind)];
		return At + 2 <= Record.size() ? ReadU16(Record, At) : 0;
	}

	uint32 FMenuGearShop::GoodsUsers(uint8 Id, EGearGoods Kind) const
	{
		const std::span<const uint8> Record = GoodsRecord(Id, Kind);
		const std::size_t At = UsersFields[static_cast<std::size_t>(Kind)];
		return At + 4 <= Record.size() ? ReadU32(Record, At) : 0;
	}

	uint32 FMenuGearShop::FittedMask(uint8 Id, EGearGoods Kind) const
	{
		if (Id == 0)
		{
			return 0;
		}
		const std::span<const uint8> Block = Services.Game->Block();
		uint32 Mask = 0;
		for (int32 Character = 0; Character < static_cast<int32>(Available.size()); ++Character)
		{
			if (!Available[static_cast<std::size_t>(Character)])
			{
				continue;
			}
			const uint8 Gear = GearOf(Character);
			const std::span<const uint8> Record = Block.subspan(GearOffset(Gear), GearSize);
			bool bFitted = false;
			switch (Kind)
			{
			case EGearGoods::Armor:
				bFitted = Record[ArmorSlot] == Id;
				break;
			case EGearGoods::Frame:
				bFitted = Record[FrameSlot] == Id;
				break;
			case EGearGoods::Engine:
				bFitted = Record[EngineSlot] == Id;
				break;
			case EGearGoods::Part:
				bFitted = Record[PartSlots] == Id || Record[PartSlots + 1] == Id || Record[PartSlots + 2] == Id;
				break;
			case EGearGoods::Weapon:
			{
				const std::size_t First = Id < FirstGun ? WeaponSlots : GunSlots;
				for (std::size_t Slot = 0; Slot < 4; ++Slot)
				{
					bFitted = bFitted || Record[First + Slot] == Id;
				}
				break;
			}
			}
			if (bFitted && Gear < 32)
			{
				Mask |= 1u << Gear;
			}
		}
		return Mask;
	}

	void FMenuGearShop::ApplyGear(uint8 Gear)
	{
		// The shop's own copy: the frame fitted without repairing, then the engine, the armor, the parts, the weapons.
		FGameState& Game = *Services.Game;
		const FGearPartTables& Parts = Resources.GearParts();
		EGearPartsCopy Copy = EGearPartsCopy::GearShop;
		uint8 Pilot = Gear < Pilots.size() ? Pilots[Gear] : uint8{0};
		if (Services.Options.bGearShopPartsAsMenu && Gear < FGearPartTables::GearCount)
		{
			// The Menu's own recompute: its sums and its pilots (the grown Emeralda once the story says so).
			Copy = EGearPartsCopy::Menu;
			const bool bGrown = (ReadU16(Game.Block(), StoryFlags) & GrownEmeraldaFlag) != 0;
			Pilot = Gear == GrownEmeraldaGear && bGrown ? GrownEmeralda : Parts.Pilot(Gear);
		}
		static_cast<void>(RefitGearFrame(Game, Gear, Parts));
		static_cast<void>(ApplyGearEngine(Game, Gear, Parts));
		static_cast<void>(ApplyGearArmor(Game, Gear, Parts));
		static_cast<void>(ApplyGearParts(Game, Gear, Parts, Pilot, Copy));
		static_cast<void>(ApplyGearWeapon(Game, Gear, Resources.GearWeapons()));
	}

	std::array<int16, 2> FMenuGearShop::GearStats(uint8 Gear)
	{
		// The story's flag gives the grown Emeralda her gear from the first comparison on.
		const std::span<const uint8> Block = Services.Game->Block();
		if ((ReadU16(Block, StoryFlags) & GrownEmeraldaFlag) != 0)
		{
			Pilots[GrownEmeraldaGear] = GrownEmeralda;
		}
		const std::span<const uint8> Record = Block.subspan(GearOffset(Gear), GearSize);
		const uint16 Defense = static_cast<uint16>(ReadU16(Record, ArmorDefense) + ReadU16(Record, PartsDefense));
		const bool bGuns = Gear == GunGears[0] || Gear == GunGears[1];
		const uint16 Weapon = bGuns ? static_cast<uint16>((Record[WeaponAttack] + Record[SecondGunAttack]) * 6 / 10) : Record[WeaponAttack];
		const uint16 Attack = static_cast<uint16>(Weapon + Record[Output] * (Record[AttackMultiplier] + Record[AttackMultiplierBonus]));
		return {static_cast<int16>(Attack), static_cast<int16>(Defense)};
	}
}
