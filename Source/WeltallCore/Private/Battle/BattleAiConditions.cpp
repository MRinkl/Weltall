#include "Battle/BattleAiMemory.h"
#include "Battle/BattleAiSteps.h"
#include "Battle/BattleContext.h"

namespace Weltall::BattleAiSteps
{
	using namespace BattleAiMemory;

	bool Condition(FBattleContext& Context, uint8 Enemy, const FWord& Word)
	{
		const uint8 A = Word.A;
		const uint8 Slot = static_cast<uint8>(Enemy + FBattleContext::PartySlotCount);
		switch (Word.Op)
		{
		case 0x81: return B(Context, Enemy, A) == Word.B;
		case 0x82: return W(Context, Enemy, A) == Word.Immediate();
		case 0x83: return B(Context, Enemy, A) <= Word.B;
		case 0x84: return W(Context, Enemy, A) <= Word.Immediate();
		case 0x85: return Word.B <= B(Context, Enemy, A);
		case 0x86: return Word.Immediate() <= W(Context, Enemy, A);
		case 0x87: return B(Context, Enemy, A) == B(Context, Enemy, Word.B);
		case 0x88: return W(Context, Enemy, A) == W(Context, Enemy, Word.B);
		case 0x89: return B(Context, Enemy, A) <= B(Context, Enemy, Word.B);
		case 0x8A: return W(Context, Enemy, A) <= W(Context, Enemy, Word.B);
		case 0x8B: return (B(Context, Enemy, A) & Word.B) != 0;
		case 0x8C: return (W(Context, Enemy, A) & Word.Immediate()) != 0;
		case 0x8D: return (B(Context, Enemy, A) & B(Context, Enemy, Word.B)) != 0;
		case 0x8E: return (W(Context, Enemy, A) & W(Context, Enemy, Word.B)) != 0;
		case 0x8F: return B(Context, Enemy, A) != Word.B;
		case 0x90: return W(Context, Enemy, A) != Word.Immediate();
		case 0x91: return B(Context, Enemy, A) != B(Context, Enemy, Word.B);
		case 0x92: return W(Context, Enemy, A) != W(Context, Enemy, Word.B);
		case 0x93: return L(Context, Enemy, A) == L(Context, Enemy, Word.B);
		case 0x94: return L(Context, Enemy, A) <= L(Context, Enemy, Word.B);
		case 0x95:
			if (A >= FBattleContext::SlotCount)
			{
				Context.Fault = EBattleFault::SlotOutsideBattle;
				return false;
			}
			return (Context.Combatants[A].Status & 0x8000) != 0;
		case 0x96:
			if (A >= Context.Occupancy.size())
			{
				Context.Fault = EBattleFault::GroupOutsideStage;
				return false;
			}
			return Context.Occupancy[A].Count == 0;
		case 0x97: return (Context.Living & 3) == 0;
		case 0x98:
			if ((Context.Living >> 5) == 0)
			{
				return true;
			}
			for (uint8 Other = FBattleContext::PartySlotCount; Other < FBattleContext::SlotCount; ++Other)
			{
				if ((Context.Slots[Other].Presence & 0x80) == 0)
				{
					return false;
				}
			}
			return true;
		case 0x9B: return (Context.Slots[Slot].Presence & 0x80) != 0;
		default: return false;
		}
	}
}
