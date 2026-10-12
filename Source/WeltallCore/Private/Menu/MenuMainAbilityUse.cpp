#include "Foundation/LittleEndian.h"
#include "Game/EtherHealing.h"
#include "Game/GameState.h"
#include "Game/GameStateLayout.h"
#include "Menu/MenuMain.h"

namespace Weltall
{
	namespace
	{
		using namespace GameStateLayout;
		using LittleEndian::ReadU16;
		using LittleEndian::ReadU32;

		constexpr int32 TargetWindow = 2;
		constexpr uint16 GearOwnWidth = 0x18;
		constexpr int32 PiecesEntry = 4;
		constexpr uint8 UnusableGrey = 0x20;
		constexpr std::size_t Power = 0x11;
		/** A healing Ether's record fields in the character's record: the Ether, its bonus, the ether buffs and the element guards. */
		constexpr std::size_t BaseEther = 0x5B;
		constexpr std::size_t EtherBonus = 0x2B;
		constexpr std::size_t EtherBuffs = 0x8A;
		constexpr std::array<std::size_t, 2> ElementWords = {0x8C, 0x8E};

		namespace Effect
		{
			constexpr uint8 Buzzer = 4;
			constexpr uint8 Used = 0x37;
		}

		void WriteU16(std::span<uint8> Block, std::size_t Offset, uint16 Value)
		{
			Block[Offset] = static_cast<uint8>(Value);
			Block[Offset + 1] = static_cast<uint8>(Value >> 8);
		}

		void WriteU32(std::span<uint8> Block, std::size_t Offset, uint32 Value)
		{
			WriteU16(Block, Offset, static_cast<uint16>(Value));
			WriteU16(Block, Offset + 2, static_cast<uint16>(Value >> 16));
		}
	}

	void FMenuMain::DimAbilityList(bool bDim)
	{
		const int32 Mode = bDim ? 1 : 0;
		DimWindow(5, bDim);
		DimWindow(6, bDim);
		for (std::size_t Entry = 0; Entry < 12; ++Entry)
		{
			if (AbilityList.Names[Entry].Quad.Corners[0].Color.R != UnusableGrey)
			{
				ShadePiece(AbilityList.Names[Entry].Quad, Mode);
				ShadePiece(AbilityList.Costs[Entry].Quad, Mode);
			}
		}
		ShadePiece(Cursors[0].Quad, Mode);
		ShadePiece(AbilityList.HelpName.Quad, Mode);
		for (FMenuLabel& Line : AbilityList.HelpLines)
		{
			ShadePiece(Line.Quad, Mode);
		}
	}

	TMenuTask<> FMenuMain::UseAbility(int32 Slot, int32 Index, int32 Kind)
	{
		const std::span<const uint8> Record = AbilityRecord(Slot, Index, Kind);
		const uint16 Targeting = Record.empty() ? 0 : ReadU16(Record, 0);
		// A gear's own ability works on the gear itself; an Ether starts on the first member.
		int32 Target = Kind == 2 ? Slot : FirstMember;
		OpenWindow(TargetWindow, 0x10, 0x0E, static_cast<uint16>(0x90 + (Kind == 2 ? GearOwnWidth : 0)), 0xB0, false, false, PiecesEntry, false);
		const uint8 Caster = Member(Slot);
		const std::span<uint8> Block = Services.Game->Block();
		const std::size_t CasterGear = GearOffset(Block[CharacterOffset(Caster) + Character::Gear]);
		bool bRedraws = true;
		uint32 Targets = 0;
		while (true)
		{
			co_await Frame();
			Targets = 0;
			if (bRedraws)
			{
				BuildAbilityList(Slot, Kind);
				DrawAbilityHelp(Slot, Index, Kind);
				DimAbilityList(true);
				DrawTargets(Kind);
				bRedraws = false;
			}
			for (std::size_t Shown = 0; Shown < 3; ++Shown)
			{
				TargetPointerShown[Shown] = false;
			}
			if ((Targeting & 1) == 0)
			{
				Targets = 1u << Target;
				TargetPointerShown[static_cast<std::size_t>(Target)] = true;
			}
			else
			{
				for (int32 Other = 0; Other < 3; ++Other)
				{
					if (Member(Other) != FGameState::EmptySlot)
					{
						Targets |= 1u << Other;
						TargetPointerShown[static_cast<std::size_t>(Other)] = true;
					}
				}
			}
			Flags[MenuFlag::TargetPointers] = 1;
			// The window stays while the cost can be paid.
			const int32 Left = Kind == 2 ? ReadU16(Block, CasterGear + GameStateLayout::Gear::Fuel) - GearOwnCost(Slot, Index)
										: ReadU16(Block, CharacterOffset(Caster) + Character::Ep) - AbilityCost(Slot, Index, Kind);
			if (Left < 0)
			{
				Targets = 0;
			}
			if (Targets == 0)
			{
				break;
			}
			switch (Code)
			{
			case EMenuCode::Up:
			case EMenuCode::Down:
				if (Kind == 0)
				{
					Target = NextMember(Target, Code == EMenuCode::Up, false);
				}
				break;
			case EMenuCode::Confirm:
			{
				// Every target that needs it is healed; the cost is paid once if any was.
				bool bHealed = false;
				for (int32 Other = 0; Other < 3; ++Other)
				{
					if ((Targets >> Other & 1) == 0)
					{
						continue;
					}
					const uint8 Healed = Member(Other);
					uint32 Hp = 0;
					uint32 MaxHp = 0;
					if (Kind == 2)
					{
						// The original asks the gear of the party slot numbered like the ability (slot 0's).
						const std::size_t Gear = GearOffset(Block[CharacterOffset(Healed) + Character::Gear]);
						const uint8 Measured = Services.Options.bGearRepairOwnGear || Member(Index) == FGameState::EmptySlot ? Healed : Member(Index);
						Hp = ReadU32(Block, Gear + GameStateLayout::Gear::Hp);
						MaxHp = ReadU32(Block, GearOffset(Block[CharacterOffset(Measured) + Character::Gear]) + GameStateLayout::Gear::MaxHp);
					}
					else
					{
						Hp = ReadU16(Block, CharacterOffset(Healed) + Character::Hp);
						MaxHp = ReadU16(Block, CharacterOffset(Healed) + Character::MaxHp);
					}
					if (Hp != MaxHp)
					{
						bHealed = true;
						ApplyAbility(Caster, Healed, Index, Kind);
					}
				}
				if (bHealed)
				{
					if (Kind == 2)
					{
						WriteU16(Block, CasterGear + GameStateLayout::Gear::Fuel,
							static_cast<uint16>(ReadU16(Block, CasterGear + GameStateLayout::Gear::Fuel) - GearOwnCost(Slot, Index)));
					}
					else
					{
						const std::size_t Ep = CharacterOffset(Caster) + Character::Ep;
						WriteU16(Block, Ep, static_cast<uint16>(ReadU16(Block, Ep) - AbilityCost(Slot, Index, Kind)));
					}
				}
				PlaySound(bHealed ? Effect::Used : Effect::Buzzer);
				bRedraws = true;
				break;
			}
			case EMenuCode::Cancel:
				Targets = 0;
				break;
			default:
				break;
			}
			if (Targets == 0)
			{
				break;
			}
		}
		for (std::size_t Shown = 0; Shown < 3; ++Shown)
		{
			TargetPointerShown[Shown] = false;
		}
		DimAbilityList(false);
		Flags[MenuFlag::TargetPanels] = 0;
		co_await Frame();
		bTargetPanelsMade = false;
		CloseWindow(TargetWindow);
	}

	void FMenuMain::ApplyAbility(uint8 Caster, uint8 Target, int32 Index, int32 Kind)
	{
		const std::span<uint8> Block = Services.Game->Block();
		if (Kind != 0)
		{
			// A gear's repair: a tenth of its maximum HP, up to it.
			const std::size_t Gear = GearOffset(Block[CharacterOffset(Caster) + Character::Gear]);
			const uint32 MaxHp = ReadU32(Block, Gear + GameStateLayout::Gear::MaxHp);
			const uint32 Hp = MaxHp / 10 + ReadU32(Block, Gear + GameStateLayout::Gear::Hp);
			WriteU32(Block, Gear + GameStateLayout::Gear::Hp, Hp > MaxHp ? MaxHp : Hp);
			return;
		}
		// The Ether's power times the healer's Ether: the original's base Ether alone, or as battle heals.
		const std::span<const uint8> Commands = Resources.CharacterCommands(Caster);
		const std::size_t PowerAt = 0x370 + static_cast<std::size_t>(Index) * 0x28 + Power;
		const uint8 Strength = PowerAt < Commands.size() ? Commands[PowerAt] : 0;
		const std::size_t Healer = CharacterOffset(Caster);
		const std::size_t Healed = CharacterOffset(Target);
		uint16 Heal = 0;
		if (Services.Options.bEtherHealsAsBattle)
		{
			const uint8 Ether = static_cast<uint8>(Block[Healer + BaseEther] + Block[Healer + EtherBonus]);
			const uint16 Guard = static_cast<uint16>(ReadU16(Block, Healed + ElementWords[0]) | ReadU16(Block, Healed + ElementWords[1]));
			Heal = static_cast<uint16>(EtherHealing(Ether, Strength, ReadU16(Block, Healer + EtherBuffs), Guard));
		}
		else
		{
			Heal = static_cast<uint16>(Block[Healer + BaseEther] * Strength);
		}
		const uint16 Hp = static_cast<uint16>(ReadU16(Block, Healed + Character::Hp) + Heal);
		const uint16 MaxHp = ReadU16(Block, Healed + Character::MaxHp);
		WriteU16(Block, Healed + Character::Hp, Hp > MaxHp ? MaxHp : Hp);
	}
}
