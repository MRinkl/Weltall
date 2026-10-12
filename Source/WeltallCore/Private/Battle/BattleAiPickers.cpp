#include "Battle/BattleAiMemory.h"
#include "Battle/BattleAiSteps.h"
#include "Battle/BattleRules.h"
#include "Game/GameState.h"

#include <array>

namespace Weltall::BattleAiSteps
{
	namespace
	{
		using namespace BattleAiMemory;
		using BattleRules::SlotBit;

		constexpr uint8 PartyEnd = FBattleContext::PartySlotCount;
		constexpr uint8 SlotEnd = FBattleContext::SlotCount;

		uint8 Self(uint8 Enemy)
		{
			return static_cast<uint8>(Enemy + FBattleContext::PartySlotCount);
		}

		/**
		 * A party member drawn at random: draws of the three slots, each rejected slot remembered, until
		 * one passes (its bit in W[Result]) or all three were rejected (0).
		 */
		template <typename FTest>
		void PickRandomParty(FBattleContext& Context, uint8 Enemy, uint8 Result, FTest&& Test)
		{
			SetW(Context, Enemy, Result, 0);
			std::array<bool, PartyEnd> Tried{};
			while (Context.Fault == EBattleFault::None)
			{
				const uint8 Slot = RandomByteBetween(*Context.Random, 0, 2);
				if (!Tried[Slot])
				{
					if (Test(Slot))
					{
						SetW(Context, Enemy, Result, SlotBit(Slot));
						return;
					}
					Tried[Slot] = true;
				}
				if (Tried[0] && Tried[1] && Tried[2])
				{
					return;
				}
			}
		}

		/** One of the slots from First to End that pass, in slot order, drawn once (no draw for none, W[Result] 0). */
		template <typename FTest>
		void PickFromList(FBattleContext& Context, uint8 Enemy, uint8 Result, uint8 First, uint8 End, FTest&& Test)
		{
			SetW(Context, Enemy, Result, 0);
			std::array<uint8, SlotEnd> Listed{};
			uint8 Count = 0;
			for (uint8 Slot = First; Slot < End; ++Slot)
			{
				if (Test(Slot))
				{
					Listed[Count++] = Slot;
				}
			}
			if (Count != 0)
			{
				SetW(Context, Enemy, Result, SlotBit(Listed[RandomByteBetween(*Context.Random, 0, static_cast<uint8>(Count - 1))]));
			}
		}

		/** The slot with the lowest key that passes, a later one winning a tie; party slot 0 when none passes. */
		template <typename FKey, typename FTest>
		void PickLowest(FBattleContext& Context, uint8 Enemy, uint8 Result, uint8 First, uint8 End, uint32 Start, FTest&& Test, FKey&& Key)
		{
			uint32 Best = Start;
			uint8 Pick = 0;
			for (uint8 Slot = First; Slot < End; ++Slot)
			{
				if (Test(Slot) && Key(Slot) <= Best)
				{
					Best = Key(Slot);
					Pick = Slot;
				}
			}
			SetW(Context, Enemy, Result, SlotBit(Pick));
		}

		/**
		 * The slot that acts soonest: its wait compared as a signed number with the best so far, which then
		 * keeps only the wait's low byte, so waits of 256 or more never win and a negative one always does.
		 */
		void PickSoonest(FBattleContext& Context, uint8 Enemy, uint8 Result, uint8 First, uint8 End, uint8 Exempt, uint8 Except)
		{
			int32 Best = 0xFF;
			uint8 Pick = 0;
			for (uint8 Slot = First; Slot < End; ++Slot)
			{
				if (IsTargetable(Context, Slot, Exempt) && static_cast<int16>(Context.Waits[Slot]) <= Best && Slot != Except)
				{
					Best = Context.Waits[Slot] & 0xFF;
					Pick = Slot;
				}
			}
			SetW(Context, Enemy, Result, SlotBit(Pick));
		}

		/** B[Result] = how many slots from First to End pass. */
		template <typename FTest>
		void Count(FBattleContext& Context, uint8 Enemy, uint8 Result, uint8 First, uint8 End, FTest&& Test)
		{
			uint8 Counted = 0;
			for (uint8 Slot = First; Slot < End; ++Slot)
			{
				if (Test(Slot))
				{
					++Counted;
				}
			}
			SetB(Context, Enemy, Result, Counted);
		}

		/** W[Result] = the bits of the slots from First to End that pass. */
		template <typename FTest>
		void Mask(FBattleContext& Context, uint8 Enemy, uint8 Result, uint8 First, uint8 End, FTest&& Test)
		{
			uint16 Bits = 0;
			for (uint8 Slot = First; Slot < End; ++Slot)
			{
				if (Test(Slot))
				{
					Bits = static_cast<uint16>(Bits | SlotBit(Slot));
				}
			}
			SetW(Context, Enemy, Result, Bits);
		}

		/** Whether a slot's word field, by the scripts' number, has a bit of the mask. */
		bool FieldHas(FBattleContext& Context, uint8 Slot, uint8 Field, uint16 Mask)
		{
			return (WordField(Context, Slot, Field) & Mask) != 0;
		}

		/** Whether a slot of the party fits a mask command's filter: 1 in a gear, 2 on foot, else any. */
		bool FitsGearFilter(const FBattleContext& Context, uint8 Slot, uint8 Filter)
		{
			return Filter == 1 ? InGear(Context, Slot) : (Filter == 2 ? !InGear(Context, Slot) : true);
		}
	}

	bool RunPicker(FBattleContext& Context, uint8 Enemy, const FWord& Word)
	{
		const uint8 A = Word.A;
		const uint8 Flag = Word.B;
		const uint8 Me = Self(Enemy);
		auto Targetable = [&Context, Flag](uint8 Slot) { return IsTargetable(Context, Slot, Flag); };
		auto SameGroup = [&Context, Me](uint8 Slot) { return Context.Slots[Me].Group == Context.Slots[Slot].Group; };
		switch (Word.Op)
		{
		case 0x40:
			PickRandomParty(Context, Enemy, A, [&](uint8 Slot) { return Targetable(Slot) && !InGear(Context, Slot); });
			return true;
		case 0x41:
			PickRandomParty(Context, Enemy, A, [&](uint8 Slot) { return Targetable(Slot) && SameGroup(Slot) && !InGear(Context, Slot); });
			return true;
		case 0x42:
			PickFromList(Context, Enemy, A, PartyEnd, SlotEnd, [&](uint8 Slot) { return Targetable(Slot) && SameGroup(Slot) && !InGear(Context, Slot); });
			return true;
		case 0x43:
			PickRandomParty(Context, Enemy, A, [&](uint8 Slot) { return Targetable(Slot) && !SameGroup(Slot) && !InGear(Context, Slot); });
			return true;
		case 0x44:
			PickFromList(Context, Enemy, A, PartyEnd, SlotEnd, [&](uint8 Slot) { return Targetable(Slot) && !SameGroup(Slot) && !InGear(Context, Slot); });
			return true;
		case 0x45: PickSoonest(Context, Enemy, A, 0, PartyEnd, Flag, SlotEnd); return true;
		case 0x46: PickSoonest(Context, Enemy, A, PartyEnd, SlotEnd, Flag, Me); return true;
		case 0x47:
			PickLowest(Context, Enemy, A, 0, PartyEnd, 0xFFFF, Targetable, [&](uint8 Slot) { return uint32{Context.Combatants[Slot].Hp}; });
			return true;
		case 0x48:
			PickLowest(Context, Enemy, A, PartyEnd, SlotEnd, 0xFFFF, Targetable, [&](uint8 Slot) { return uint32{Context.Combatants[Slot].Hp}; });
			return true;
		case 0x49:
			PickRandomParty(Context, Enemy, A, [&](uint8 Slot) { return Targetable(Slot) && InGear(Context, Slot) && SameGroup(Slot); });
			return true;
		case 0x4A:
			PickRandomParty(Context, Enemy, A, [&](uint8 Slot) { return Targetable(Slot) && InGear(Context, Slot); });
			return true;
		case 0x4B:
			PickFromList(Context, Enemy, A, PartyEnd, SlotEnd, [&](uint8 Slot) { return Targetable(Slot) && InGear(Context, Slot); });
			return true;
		case 0x4C:
			Count(Context, Enemy, A, 0, PartyEnd, [&](uint8 Slot) { return IsTargetable(Context, Slot, 0) && Context.Slots[Slot].Group == Word.B; });
			return true;
		case 0x4D:
			Count(Context, Enemy, A, PartyEnd, SlotEnd, [&](uint8 Slot) { return IsTargetable(Context, Slot, 0) && Context.Slots[Slot].Group == Word.B; });
			return true;
		case 0x4E:
		{
			const uint8 Other = Group(Context, FirstSlot(W(Context, Enemy, Word.B)));
			if (Other >= FBattleStage::GroupCount)
			{
				Context.Fault = EBattleFault::GroupOutsideStage;
			}
			if (Context.Fault == EBattleFault::None)
			{
				SetB(Context, Enemy, A, Context.Stage.Route(Context.Slots[Me].Group, Other, 0));
			}
			return true;
		}
		case 0x4F:
			Count(Context, Enemy, A, 0, PartyEnd, [&](uint8 Slot) {
				return IsTargetable(Context, Slot, 0) && Context.Slots[Slot].Group == Group(Context, FirstSlot(W(Context, Enemy, Word.B)));
			});
			return true;
		case 0x50:
			Count(Context, Enemy, A, PartyEnd, SlotEnd, [&](uint8 Slot) {
				return IsTargetable(Context, Slot, 0) && Context.Slots[Slot].Group == Group(Context, FirstSlot(W(Context, Enemy, Word.B)));
			});
			return true;
		case 0x51:
			SetB(Context, Enemy, A, 0);
			for (std::size_t Cell = 0; Cell < Context.ListedItems.size(); ++Cell)
			{
				if (Context.ListedItems[Cell] == Word.B)
				{
					SetB(Context, Enemy, A, Context.ListedCounts[Cell]);
					break;
				}
			}
			return true;
		case 0x53: SetL(Context, Enemy, A, Context.GameState->Gold()); return true;
		case 0x54:
			PickFromList(Context, Enemy, A, 0, SlotEnd, [&](uint8 Slot) { return IsPresent(Context, Slot) && Context.Combatants[Slot].Character == Word.B; });
			return true;
		case 0x55: SetL(Context, Enemy, A, Context.Action.Results[Me]); return true;
		case 0x56:
			PickFromList(Context, Enemy, A, PartyEnd, SlotEnd,
				[&](uint8 Slot) { return IsTargetableOrHidden(Context, Slot, Flag) && (Context.Slots[Slot].Presence & 0x80) != 0; });
			return true;
		case 0x57:
			PickRandomParty(Context, Enemy, A, [&](uint8 Slot) {
				const uint16 Status = Context.Combatants[Slot].Status;
				return (Status & 0x8000) != 0 && (Status & 0x4002) == 0;
			});
			return true;
		case 0x58:
			Count(Context, Enemy, A, 0, PartyEnd, [&](uint8 Slot) { return (Context.Combatants[Slot].Status & 0xC000) == 0; });
			return true;
		case 0x59:
			Count(Context, Enemy, A, PartyEnd, SlotEnd, [&](uint8 Slot) {
				return Context.InBattle[Slot] && (Context.Combatants[Slot].Status & 0xC000) == 0 && Context.Slots[Slot].Presence == 0;
			});
			return true;
		case 0x5A:
			PickLowest(Context, Enemy, A, 0, PartyEnd, 0xFFFFFFFF, [&](uint8 Slot) { return Targetable(Slot) && InGear(Context, Slot); },
				[&](uint8 Slot) { return Context.Combatants[Slot].GearPart.Hp; });
			return true;
		case 0x5B:
			PickLowest(Context, Enemy, A, PartyEnd, SlotEnd, 0xFFFFFFFF, [&](uint8 Slot) { return Targetable(Slot) && InGear(Context, Slot); },
				[&](uint8 Slot) { return uint32{Context.Combatants[Slot].Hp}; });
			return true;
		case 0x5C:
		case 0x5D:
		case 0x5E:
		case 0x5F:
		case 0x60:
		case 0x61:
		{
			const bool bEnemies = Word.Op == 0x5D || Word.Op == 0x5F;
			const bool bInGear = Word.Op == 0x5E || Word.Op == 0x5F || Word.Op == 0x61;
			const uint8 Exempt = Word.Op >= 0x60 ? 1 : 0;
			PickFromList(Context, Enemy, Word.B, bEnemies ? PartyEnd : 0, bEnemies ? SlotEnd : PartyEnd, [&](uint8 Slot) {
				return IsTargetable(Context, Slot, Exempt) && (!bInGear || InGear(Context, Slot)) && FieldHas(Context, Slot, A, W(Context, Enemy, Word.C));
			});
			return true;
		}
		case 0x64: SetW(Context, Enemy, A, SlotBit(Me)); return true;
		case 0x65: Mask(Context, Enemy, A, 0, PartyEnd, [&](uint8 Slot) { return IsPresent(Context, Slot) && FitsGearFilter(Context, Slot, Word.B); }); return true;
		case 0x66: Mask(Context, Enemy, A, PartyEnd, SlotEnd, [&](uint8 Slot) { return IsPresent(Context, Slot) && FitsGearFilter(Context, Slot, Word.B); }); return true;
		case 0x67:
			Mask(Context, Enemy, A, 0, PartyEnd, [&](uint8 Slot) {
				return IsPresent(Context, Slot) && Group(Context, FirstSlot(W(Context, Enemy, Word.B))) == Context.Slots[Slot].Group;
			});
			return true;
		case 0x68:
			Mask(Context, Enemy, A, PartyEnd, SlotEnd, [&](uint8 Slot) {
				return IsPresent(Context, Slot) && Group(Context, FirstSlot(W(Context, Enemy, Word.B))) == Context.Slots[Slot].Group;
			});
			return true;
		default:
			return false;
		}
	}
}
