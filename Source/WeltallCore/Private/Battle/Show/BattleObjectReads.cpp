#include "BattleObjectMachine.h"

#include "Battle/BattleContext.h"

#include <array>
#include <string>
#include <type_traits>

namespace Weltall::BattleObjectMachine
{
	namespace
	{
		constexpr uint32 DefaultRecordNumbers = 0xFE;
		constexpr uint32 StanceFromState = 0xFF;
		constexpr uint8 MirroredStance = 0x80;
		constexpr uint32 FirstExtensionAnimation = 0x40;
		constexpr uint8 NoTargetSlot = 13;
		constexpr std::size_t ResultsOffset = 0x18;
		/** The original's slot records (0x1C bytes, x, z and a height never written at +0Ah, +0Ch, +0Eh) and the performance records after them (0x48 bytes). */
		constexpr std::size_t SlotRecordSize = 0x1C;
		constexpr std::size_t SlotRecordsSize = FBattleContext::SlotCount * SlotRecordSize;
		constexpr std::array<std::size_t, 3> SlotPlaceOffsets = {0x0A, 0x0E, 0x0C};
		constexpr std::size_t PerformanceRecordSize = 0x48;
		/** The stances animation FFh picks from the combatant's state. */
		constexpr uint8 BrokenAnimation = 0x1B;
		constexpr uint8 WeakAnimation = 6;
		constexpr uint8 StandingAnimation = 1;
		constexpr uint8 Mirrored = 1;
		constexpr uint8 Weak = 2;
		constexpr uint8 AtFullAttack = 4;
		constexpr uint16 MirroredStatus = 0x400;
		constexpr uint16 NeverWeak = 0x0001;
		constexpr uint8 FullAttackLevel = 4;
		/** The code of a record the playback passes over, and of the end of the records. */
		constexpr uint8 PassedOverCode = 0xF7;
		constexpr uint8 EndCode = 0xFF;
		constexpr int32 NoMoreRecords = -2;
		/** The occupancy's groups of the enemies' side in gears come after the three others' eight each. */
		constexpr int32 EnemyGearGroups = 24;
		/** Where the gear effects' bytes start in a combatant's record. */
		constexpr std::size_t GearEffectBytes = 0x101;

		/** The byte of a performance record at Offset of its layout (the result classes run past their eleven into the actor and the totals). */
		uint8 PerformanceByte(const FBattlePerformance& Record, std::size_t Offset)
		{
			if (Offset < 0x16)
			{
				return static_cast<uint8>(Record.Amounts[Offset / 2] >> (8 * (Offset % 2)));
			}
			if (Offset < 0x18)
			{
				return static_cast<uint8>(Record.Targets >> (8 * (Offset % 2)));
			}
			if (Offset < 0x23)
			{
				return Record.Results[Offset - 0x18];
			}
			if (Offset == 0x23)
			{
				return Record.Actor;
			}
			if (Offset < 0x3A)
			{
				return static_cast<uint8>(Record.Totals[(Offset - 0x24) / 2] >> (8 * (Offset % 2)));
			}
			if (Offset < 0x3C)
			{
				return static_cast<uint8>(Record.Parameter >> (8 * (Offset % 2)));
			}
			if (Offset < 0x47)
			{
				return Record.TotalKinds[Offset - 0x3C];
			}
			return Record.Code;
		}

		/** The byte at Offset of a slot's record: those the battle never writes (7-9, the height at 0Eh and all after it) are 0, its memory starting cleared. */
		uint8 SlotRecordByte(const FBattleSlot& Slot, std::size_t Offset)
		{
			switch (Offset)
			{
			case 0: return Slot.Group;
			case 1: return Slot.Place;
			case 2: return Slot.Occupant;
			case 3: return Slot.Presence;
			case 4: return Slot.InGear;
			case 5: return Slot.Byte5;
			case 6: return Slot.FacesLeft;
			case 0x0A: return static_cast<uint8>(Slot.Position.X);
			case 0x0B: return static_cast<uint8>(Slot.Position.X >> 8);
			case 0x0C: return static_cast<uint8>(Slot.Position.Z);
			case 0x0D: return static_cast<uint8>(Slot.Position.Z >> 8);
			default: return 0;
			}
		}

		/**
		 * The byte at Offset of the slot records and the performance records that follow them in memory, which the
		 * objects read as one run (a slot past the eleven, the result of the record before the first); none past the last record.
		 */
		std::optional<uint8> RecordRunByte(const FBattleContext& Context, std::size_t Offset)
		{
			if (Offset < SlotRecordsSize)
			{
				return SlotRecordByte(Context.Slots[Offset / SlotRecordSize], Offset % SlotRecordSize);
			}
			const std::size_t Index = (Offset - SlotRecordsSize) / PerformanceRecordSize;
			if (Index >= static_cast<std::size_t>(FBattleContext::PerformanceCount))
			{
				return std::nullopt;
			}
			return PerformanceByte(Context.Performances[Index], (Offset - SlotRecordsSize) % PerformanceRecordSize);
		}

		/** The byte at Offset of a combatant's record, as the port keeps its members; none where the port keeps nothing. */
		std::optional<uint8> CombatantByte(const FBattleCombatant& Combatant, std::size_t Offset)
		{
			const std::size_t ModelBytes = FBattleCombatant::CharacterRecordSize + FBattleGearPart::ModelBytesOffset;
			if (Offset >= ModelBytes && Offset < ModelBytes + Combatant.GearPart.ModelBytes.size())
			{
				return Combatant.GearPart.ModelBytes[Offset - ModelBytes];
			}
			std::optional<uint8> Found;
			VisitBattleCombatantLayout(Combatant, [&](std::size_t At, const auto& Member) {
				using FMember = std::remove_cvref_t<decltype(Member)>;
				if (Offset >= At && Offset < At + sizeof(FMember))
				{
					Found = static_cast<uint8>(static_cast<std::make_unsigned_t<FMember>>(Member) >> (8 * (Offset - At)));
				}
			});
			return Found;
		}

		const FBattleCombatant* Combatant(FBattleObjects& Objects, int32 Slot, const char* What)
		{
			if (Slot < 0 || Slot >= FBattleContext::SlotCount)
			{
				Objects.Fail(std::string("the objects read ") + What + " of slot " + std::to_string(Slot) + ", past the combatants");
				return nullptr;
			}
			return &Objects.Context->Combatants[static_cast<std::size_t>(Slot)];
		}
	}

	std::optional<std::array<int16, 3>> SlotPlace(FBattleObjects& Objects, int32 Slot)
	{
		if (Slot < 0)
		{
			Objects.Fail("the objects read the place of slot " + std::to_string(Slot) + ", before the slot records");
			return std::nullopt;
		}
		// Past the eleven the original reads on into the performance records that follow them in memory (its
		// sub-objects, clones and stage have no record of their own).
		std::array<int16, 3> Place{};
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			const std::size_t At = static_cast<std::size_t>(Slot) * SlotRecordSize + SlotPlaceOffsets[Axis];
			const std::optional<uint8> Low = RecordRunByte(*Objects.Context, At);
			const std::optional<uint8> High = RecordRunByte(*Objects.Context, At + 1);
			if (!Low || !High)
			{
				Objects.Fail("the objects read the place of slot " + std::to_string(Slot) + ", past the performance records after the slot records");
				return std::nullopt;
			}
			Place[Axis] = static_cast<int16>(*Low | *High << 8);
		}
		return Place;
	}

	uint32 FirstTarget(const FBattleObjects& Objects)
	{
		uint32 Slot = 0;
		while (Slot < NoTargetSlot && ((Objects.PlayTargets >> Slot) & 1) == 0)
		{
			++Slot;
		}
		return Slot;
	}

	uint32 ResolveWho(const FBattleObjects& Objects, const FBattleObject& Object, uint32 Who, uint16& Mask)
	{
		uint32 Slot = Who & 0xFF;
		switch (Slot)
		{
		case 0xFF:
			Slot = FirstTarget(Objects) & 0xFF;
			break;
		case 0xFE:
			Slot = Objects.Performer & 0xFF;
			break;
		case 0xFD:
		case 0xF9:
			Slot = Object.Slot;
			break;
		case 0xFC:
			Slot = Object.Owner;
			break;
		case 0xFA:
			Slot = FBattleObjects::StageSlot;
			break;
		case 0xF8:
			Slot = (Object.Slot * 2u + FBattleObjects::FirstSubObject) & 0xFF;
			break;
		case 0xF7:
			Slot = (Object.Slot * 2u + FBattleObjects::FirstSubObject + 1) & 0xFF;
			break;
		default:
			break;
		}
		Mask = static_cast<uint16>(1u << (Slot & 0x1F));
		return Slot;
	}

	std::optional<uint8> ResultClass(FBattleObjects& Objects, int32 Slot)
	{
		// Before the first record is played (the play record 0) the result is read from the slot records before it.
		const int32 Record = Objects.Services->PlayRecord() - 1;
		const int64 At = int64{SlotRecordsSize} + int64{Record} * int64{PerformanceRecordSize} + int64{ResultsOffset} + Slot;
		const std::optional<uint8> Result = At >= 0 ? RecordRunByte(*Objects.Context, static_cast<std::size_t>(At)) : std::nullopt;
		if (!Result)
		{
			Objects.Fail("the objects read the result of record " + std::to_string(Record) + ", past the records");
		}
		return Result;
	}

	bool ResultMatches(FBattleObjects& Objects, int32 Slot, uint8 Filter)
	{
		const std::optional<uint8> Result = ResultClass(Objects, Slot);
		if (!Result)
		{
			return false;
		}
		bool bMatches = false;
		switch (*Result)
		{
		case 0:
		case 1:
			bMatches = (Filter & 1) == 0;
			break;
		case 2:
		case 3:
			bMatches = (Filter & 8) == 0;
			break;
		case 4:
			bMatches = (Filter & 4) == 0;
			break;
		case 5:
			bMatches = (Filter & 2) == 0;
			break;
		default:
			break;
		}
		return bMatches || Filter == 0;
	}

	std::optional<FGearRecord> Animation(FBattleObjects& Objects, FBattleObject& Object, uint32 Number, bool& bMirrored)
	{
		bMirrored = false;
		Number &= 0xFF;
		if (Number >= DefaultRecordNumbers)
		{
			if (Number == StanceFromState)
			{
				const std::optional<uint8> Flags = StanceFlags(Objects, Object.Slot);
				if (!Flags)
				{
					return std::nullopt;
				}
				// A stance the flags pick nothing for stays as it was, its mirrored bit included.
				if ((*Flags & AtFullAttack) != 0 && (Object.Flags & BattleObjectFlags::BrokenStance) != 0)
				{
					Object.Stance = BrokenAnimation;
				}
				else if ((*Flags & Weak) != 0 && (Object.Flags & BattleObjectFlags::WeakStance) != 0)
				{
					if ((Objects.Context->Combatants[Object.Slot].Absorption & NeverWeak) == 0)
					{
						Object.Stance = WeakAnimation;
					}
				}
				else if ((Object.Flags & BattleObjectFlags::OtherStance) == 0)
				{
					Object.Stance = StandingAnimation;
				}
				if ((*Flags & Mirrored) != 0)
				{
					Object.Stance = static_cast<uint8>(Object.Stance | MirroredStance);
				}
			}
			Number = Object.Stance & ~MirroredStance & 0xFFu;
			bMirrored = (Object.Stance & MirroredStance) != 0;
		}
		FGearMotionPack* Pack = nullptr;
		std::optional<uint32> At;
		if (Number < FirstExtensionAnimation)
		{
			Pack = Object.bHasSoundTable ? Object.Motion : nullptr;
			At = Pack != nullptr ? Pack->Record(static_cast<int32>(Number)) : std::nullopt;
		}
		else if (Object.Extension != nullptr && Object.bExtensionInstalled)
		{
			Pack = &Object.Extension->Pack;
			At = Pack->Record(static_cast<int32>(Number - FirstExtensionAnimation));
		}
		// An empty entry points at the bank itself once relocated: the original takes its words for a record.
		if (!At || *At == Pack->Bank)
		{
			return std::nullopt;
		}
		return GearMachine::ReadRecord(Objects, {Pack, *At});
	}

	std::optional<uint32> SoundBase(FBattleObjects& Objects, const FBattleObject& Object, uint32 Base)
	{
		std::optional<uint16> Set;
		switch (Base)
		{
		case 0:
			return Objects.Services->SystemSoundBase();
		case 1:
			if (Object.bHasSoundTable && Object.Motion != nullptr)
			{
				Set = SoundFileId(*Object.Motion);
			}
			break;
		case 2:
			Set = Object.ExtensionSoundSet;
			break;
		case 3:
			return Objects.Services->EventSoundBase();
		default:
			// The original returns the number itself here, unshifted.
			return 3;
		}
		if (!Set)
		{
			Objects.Fail("object " + std::to_string(Object.Slot) + " plays sound set " + std::to_string(Base) + ", which it has none of");
			return std::nullopt;
		}
		return static_cast<uint32>(*Set) << 16;
	}

	uint16 KnockedOutSlots(const FBattleObjects& Objects)
	{
		return Objects.Context->Action.KnockedOut;
	}

	std::optional<int32> PeekCode(FBattleObjects& Objects, int32 Record)
	{
		for (int32 At = Record; At < FBattleContext::PerformanceCount; ++At)
		{
			const uint8 Code = Objects.Context->Performances[static_cast<std::size_t>(At)].Code;
			if (Code != PassedOverCode)
			{
				return Code == EndCode ? NoMoreRecords : static_cast<int32>(static_cast<int8>(Code));
			}
		}
		Objects.Fail("the objects look for a record's code past the records");
		return std::nullopt;
	}

	std::optional<uint8> SlotGroup(FBattleObjects& Objects, int32 Slot)
	{
		if (Slot < 0 || Slot >= FBattleContext::SlotCount)
		{
			Objects.Fail("the objects read the group of slot " + std::to_string(Slot) + ", past the slot records");
			return std::nullopt;
		}
		return Objects.Context->Slots[static_cast<std::size_t>(Slot)].Group;
	}

	std::optional<uint8> GearGroupCount(FBattleObjects& Objects, int32 Slot)
	{
		const std::optional<uint8> Group = SlotGroup(Objects, Slot);
		if (!Group)
		{
			return std::nullopt;
		}
		const std::size_t Index = static_cast<std::size_t>(*Group) + EnemyGearGroups;
		if (Index >= Objects.Context->Occupancy.size())
		{
			Objects.Fail("the objects read the count of group " + std::to_string(*Group) + ", past the occupancy");
			return std::nullopt;
		}
		return Objects.Context->Occupancy[Index].Count;
	}

	std::optional<uint8> StanceFlags(FBattleObjects& Objects, int32 Slot)
	{
		const FBattleCombatant* Found = Combatant(Objects, Slot, "the stance");
		if (Found == nullptr)
		{
			return std::nullopt;
		}
		uint8 Flags = (Found->GearPart.Status & MirroredStatus) != 0 ? Mirrored : 0;
		if (Found->GearPart.Hp < Found->GearPart.MaxHp >> 3 && (Found->Absorption & NeverWeak) == 0)
		{
			Flags |= Weak;
		}
		if (Found->AttackLevel == FullAttackLevel)
		{
			Flags |= AtFullAttack;
		}
		return Flags;
	}

	std::optional<uint8> GearEffectBase(FBattleObjects& Objects, int32 Slot, int32 Number)
	{
		// The byte past the record belongs to the next combatant's.
		std::size_t Offset = GearEffectBytes + static_cast<std::size_t>(Number);
		const int32 Owner = Slot + static_cast<int32>(Offset / FBattleCombatant::RecordSize);
		Offset %= FBattleCombatant::RecordSize;
		const FBattleCombatant* Found = Combatant(Objects, Owner, "a gear effect's byte");
		if (Found == nullptr)
		{
			return std::nullopt;
		}
		const std::optional<uint8> Byte = CombatantByte(*Found, Offset);
		if (!Byte)
		{
			Objects.Fail("key event 1 reads byte " + std::to_string(Offset) + " of a combatant's record, which the port does not keep");
		}
		return Byte;
	}

	EBattleClutCurve ClutCurve(uint32 Number)
	{
		switch (Number)
		{
		case 1:
			return EBattleClutCurve::Linear;
		case 2:
			return EBattleClutCurve::Down;
		case 3:
			return EBattleClutCurve::Up;
		default:
			return EBattleClutCurve::Cosine;
		}
	}
}
