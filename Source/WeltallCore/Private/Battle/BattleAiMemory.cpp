#include "Battle/BattleAiMemory.h"

#include "Battle/BattleRules.h"
#include "Game/GameState.h"

namespace Weltall::BattleAiMemory
{
	namespace
	{
		constexpr std::size_t RecordSize = 0x40;
		constexpr std::size_t VariablesAt = 0x10;
		constexpr std::size_t LongsAt = 0x10;
		constexpr std::size_t WordsAt = 0x20;
		constexpr std::size_t BytesAt = 0x30;
		constexpr uint8 NoSlot = FBattleContext::SlotCount;

		/** The bytes of a variable Size wide at an enemy's record offset, or null past the records or over scripts. */
		uint8* Variable(FBattleContext& Context, uint8 Enemy, std::size_t Offset)
		{
			const std::size_t Absolute = static_cast<std::size_t>(Enemy) * RecordSize + Offset;
			const std::size_t Record = Absolute / RecordSize;
			const std::size_t Within = Absolute % RecordSize;
			if (Record >= Context.AiRecords.size() || Within < VariablesAt)
			{
				Context.Fault = EBattleFault::AiVariableOutsideRecords;
				return nullptr;
			}
			return &Context.AiRecords[Record].Variables[Within - VariablesAt];
		}

		FBattleCombatant* Combatant(FBattleContext& Context, uint8 Slot)
		{
			if (Slot >= FBattleContext::SlotCount)
			{
				Context.Fault = EBattleFault::SlotOutsideBattle;
				return nullptr;
			}
			return &Context.Combatants[Slot];
		}

		uint8* BytePointer(FBattleCombatant& C, uint8 Field)
		{
			FBattleGearPart& G = C.GearPart;
			switch (Field)
			{
			case 0: return &C.Weapons[0].Attack;
			case 1: return &C.Character;
			case 2: return &G.Output;
			case 3: return &G.AttackMultiplier;
			case 4: return &C.Attack;
			case 5: return &C.Defense;
			case 6: return &C.Agility;
			case 7: return &C.EquipmentBonuses[5];
			case 8: return &C.Byte5D;
			case 9: return &C.Ether;
			case 10: return &C.EtherDefense;
			case 11: return &C.Hit;
			case 12: return &C.Evade;
			case 13: return &C.StatusRate;
			case 14: return &C.Byte61;
			case 15: return &C.Bytes64[0];
			case 16: return &C.Bytes64[1];
			case 17: return &C.Bytes64[2];
			case 18: return &C.Bytes64[3];
			case 19: return &G.Agility;
			case 20: return &G.Weapons[0].Attack;
			case 21: return &G.Guard;
			case 22: return &G.Byte9D;
			case 23: return &G.EtherAmplifier;
			default: return nullptr;
			}
		}

		uint16* WordPointer(FBattleCombatant& C, uint8 Field)
		{
			FBattleGearPart& G = C.GearPart;
			switch (Field)
			{
			case 0: return &C.MaxHp;
			case 1: return &C.Hp;
			case 2: return &C.Status;
			case 3: return &C.StatusImmunity;
			case 4: return &C.Mental;
			case 5: return &C.MentalImmunity;
			case 6: return &C.Buffs;
			case 7: return &C.PermanentBuffs;
			case 8: return &C.EtherBuffs;
			case 9: return &C.PermanentEtherBuffs;
			case 10: return &C.Elements;
			case 11: return &C.PermanentElements;
			case 12: return &G.Word6C;
			case 13: return &G.Defense;
			case 14: return &G.EtherDefense;
			case 15: return &G.Status;
			case 16: return &G.StatusImmunity;
			case 17: return &G.Buffs;
			case 18: return &G.PermanentBuffs;
			case 19: return &G.Elements;
			case 20: return &G.PermanentElements;
			case 21: return &C.Traits;
			case 22: return &C.Absorption;
			case 23: return &C.Weakness;
			default: return nullptr;
			}
		}

		uint8* ByteFieldPointer(FBattleContext& Context, uint8 Slot, uint8 Field)
		{
			FBattleCombatant* C = Combatant(Context, Slot);
			if (C == nullptr)
			{
				return nullptr;
			}
			uint8* Pointer = BytePointer(*C, Field);
			if (Pointer == nullptr)
			{
				Context.Fault = EBattleFault::IndexOutsideTable;
			}
			return Pointer;
		}

		/** Where each word field lies in a combatant, by its number. */
		constexpr uint8 WordFieldCount = 24;
		constexpr std::array<uint16, WordFieldCount> WordFieldOffsets{0x4E, 0x4C, 0x7C, 0x7E, 0x80, 0x82, 0x84, 0x86, 0x88, 0x8A, 0x8C, 0x8E,
			0x110, 0x114, 0x116, 0x120, 0x122, 0x124, 0x126, 0x128, 0x12A, 0x34, 0x36, 0x38};

		/**
		 * An empty mask's slot is the eleventh combatant's, the memory after the ten: among it the drops and the new
		 * stats the result screen fills, which hold zeros while the battle is fought.
		 */
		constexpr bool ResultFillsPastCombatants(uint16 Offset)
		{
			return (Offset >= 0x3C && Offset + 1 < 0x58) || (Offset >= 0x70 && Offset + 1 < 0x88);
		}

		uint16* WordFieldPointer(FBattleContext& Context, uint8 Slot, uint8 Field)
		{
			FBattleCombatant* C = Combatant(Context, Slot);
			if (C == nullptr)
			{
				return nullptr;
			}
			uint16* Pointer = WordPointer(*C, Field);
			if (Pointer == nullptr)
			{
				Context.Fault = EBattleFault::IndexOutsideTable;
			}
			return Pointer;
		}
	}

	uint8 B(FBattleContext& Context, uint8 Enemy, uint8 Index)
	{
		const uint8* Bytes = Variable(Context, Enemy, BytesAt + Index);
		return Bytes != nullptr ? Bytes[0] : 0;
	}

	void SetB(FBattleContext& Context, uint8 Enemy, uint8 Index, uint8 Value)
	{
		if (uint8* Bytes = Variable(Context, Enemy, BytesAt + Index))
		{
			Bytes[0] = Value;
		}
	}

	uint16 W(FBattleContext& Context, uint8 Enemy, uint8 Index)
	{
		const uint8* Bytes = Variable(Context, Enemy, WordsAt + 2u * Index);
		return Bytes != nullptr ? static_cast<uint16>(Bytes[0] | Bytes[1] << 8) : 0;
	}

	void SetW(FBattleContext& Context, uint8 Enemy, uint8 Index, uint16 Value)
	{
		if (uint8* Bytes = Variable(Context, Enemy, WordsAt + 2u * Index))
		{
			Bytes[0] = static_cast<uint8>(Value);
			Bytes[1] = static_cast<uint8>(Value >> 8);
		}
	}

	uint32 L(FBattleContext& Context, uint8 Enemy, uint8 Index)
	{
		const uint8* Bytes = Variable(Context, Enemy, LongsAt + 4u * Index);
		return Bytes != nullptr ? static_cast<uint32>(Bytes[0]) | static_cast<uint32>(Bytes[1]) << 8 | static_cast<uint32>(Bytes[2]) << 16
				| static_cast<uint32>(Bytes[3]) << 24
								: 0;
	}

	void SetL(FBattleContext& Context, uint8 Enemy, uint8 Index, uint32 Value)
	{
		if (uint8* Bytes = Variable(Context, Enemy, LongsAt + 4u * Index))
		{
			for (std::size_t Byte = 0; Byte < 4; ++Byte)
			{
				Bytes[Byte] = static_cast<uint8>(Value >> (8 * Byte));
			}
		}
	}

	uint16 Global(FBattleContext& Context, uint8 Index)
	{
		if (Index >= FGameState::BattleGlobalCount)
		{
			Context.Fault = EBattleFault::IndexOutsideTable;
			return 0;
		}
		return Context.GameState->BattleGlobals()[Index];
	}

	void SetGlobal(FBattleContext& Context, uint8 Index, uint16 Value)
	{
		if (Index >= FGameState::BattleGlobalCount)
		{
			Context.Fault = EBattleFault::IndexOutsideTable;
			return;
		}
		Context.GameState->BattleGlobals()[Index] = Value;
	}

	uint8 FirstSlot(uint16 Mask)
	{
		for (uint8 Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
		{
			if ((Mask & BattleRules::SlotBit(Slot)) != 0)
			{
				return Slot;
			}
		}
		return NoSlot;
	}

	uint8 ByteField(FBattleContext& Context, uint8 Slot, uint8 Field)
	{
		const uint8* Pointer = ByteFieldPointer(Context, Slot, Field);
		return Pointer != nullptr ? *Pointer : 0;
	}

	void SetByteField(FBattleContext& Context, uint8 Slot, uint8 Field, uint8 Value)
	{
		if (uint8* Pointer = ByteFieldPointer(Context, Slot, Field))
		{
			*Pointer = Value;
		}
	}

	uint16 WordField(FBattleContext& Context, uint8 Slot, uint8 Field)
	{
		if (Slot == NoSlot && Field < WordFieldCount && ResultFillsPastCombatants(WordFieldOffsets[Field]))
		{
			return 0;
		}
		const uint16* Pointer = WordFieldPointer(Context, Slot, Field);
		return Pointer != nullptr ? *Pointer : 0;
	}

	void SetWordField(FBattleContext& Context, uint8 Slot, uint8 Field, uint16 Value)
	{
		if (uint16* Pointer = WordFieldPointer(Context, Slot, Field))
		{
			*Pointer = Value;
		}
	}

	uint8 Group(FBattleContext& Context, uint8 Slot)
	{
		// The slot records are followed by the show's records, so the group of slot 11 (an empty mask's)
		// is the low byte of the first show record's amount for slot 0.
		if (Slot == NoSlot)
		{
			return static_cast<uint8>(Context.Performances[0].Amounts[0]);
		}
		if (Slot > NoSlot)
		{
			Context.Fault = EBattleFault::SlotOutsideBattle;
			return 0;
		}
		return Context.Slots[Slot].Group;
	}

	bool IsTargetableOrHidden(const FBattleContext& Context, uint8 Slot, uint8 bExemptAllowed)
	{
		const FBattleCombatant& C = Context.Combatants[Slot];
		if (!Context.InBattle[Slot] || (C.Status & 0xC002) != 0)
		{
			return false;
		}
		return bExemptAllowed != 0 || (C.Buffs & 0x20) == 0;
	}

	bool IsTargetable(const FBattleContext& Context, uint8 Slot, uint8 bExemptAllowed)
	{
		return Context.Slots[Slot].Presence == 0 && IsTargetableOrHidden(Context, Slot, bExemptAllowed);
	}

	bool IsPresent(const FBattleContext& Context, uint8 Slot)
	{
		return Context.InBattle[Slot] && Context.Slots[Slot].Presence == 0 && (Context.Combatants[Slot].Status & 0xC000) == 0;
	}

	bool InGear(const FBattleContext& Context, uint8 Slot)
	{
		return Context.ApRecords[Slot].InGear != 0;
	}

	uint8 QueueByte(FBattleContext& Context, std::size_t Offset)
	{
		if (Offset >= Context.AiQueue.size())
		{
			Context.Fault = EBattleFault::EntryOutsideQueue;
			return 0;
		}
		return Context.AiQueue[Offset];
	}

	void SetQueueByte(FBattleContext& Context, std::size_t Offset, uint8 Value)
	{
		if (Offset >= Context.AiQueue.size())
		{
			Context.Fault = EBattleFault::EntryOutsideQueue;
			return;
		}
		Context.AiQueue[Offset] = Value;
	}

	uint16 QueueWord(FBattleContext& Context, std::size_t Offset)
	{
		return static_cast<uint16>(QueueByte(Context, Offset) | QueueByte(Context, Offset + 1) << 8);
	}
}
