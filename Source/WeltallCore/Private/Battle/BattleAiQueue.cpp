#include "Battle/BattleAction.h"
#include "Battle/BattleAi.h"
#include "Battle/BattleAiMemory.h"
#include "Battle/BattleAiSteps.h"
#include "Battle/BattleRules.h"
#include "Battle/BattleSteps.h"

namespace Weltall
{
	using namespace BattleAiMemory;
	using BattleRules::SlotBit;

	namespace
	{
		constexpr std::size_t EntrySize = 8;
		constexpr uint8 EntryCount = 32;
		constexpr uint8 LastMessageBox = 8;
		/** The wait an act's message is held before the act. */
		constexpr uint8 MessageWait = 0x1E;

		/** The show's codes the queue writes. */
		constexpr uint8 GearCloseCode = 0x1B;
		constexpr uint8 EffectCode = 0xF4;
		constexpr uint8 CameraCode = 0xF6;
		constexpr uint8 WaitCode = 0xF7;
		constexpr uint8 CloseMessageCode = 0xF8;
		constexpr uint8 EscapeCode = 0xF9;
		constexpr uint8 MessageCode = 0xFA;
		constexpr uint8 TransformCode = 0xFB;
		constexpr uint8 StanceCode = 0xFC;
		constexpr uint8 MoveCode = 0xFD;
		constexpr uint8 EndCode = 0xFE;
		constexpr uint8 EmptyCode = 0xFF;
		constexpr uint8 NeverAgain = 0xFF;

		/** An entry's fields: type, command or field, code, message, value, targets. */
		struct FEntry
		{
			uint8 Type = 0;
			uint8 Command = 0;
			uint8 Code = 0;
			uint8 Message = 0;
			uint16 Value = 0;
			uint16 Targets = 0;
		};

		FEntry ReadEntry(FBattleContext& Context, uint8 Index)
		{
			const std::size_t At = Index * EntrySize;
			return {QueueByte(Context, At), QueueByte(Context, At + 1), QueueByte(Context, At + 2), QueueByte(Context, At + 3),
				QueueWord(Context, At + 4), QueueWord(Context, At + 6)};
		}

		/** The show's current record; past the 32 faults. */
		FBattlePerformance* Record(FBattleContext& Context)
		{
			if (Context.PerformanceIndex >= FBattleContext::PerformanceCount)
			{
				Context.Fault = EBattleFault::PerformanceOutsideRecords;
				return nullptr;
			}
			return &Context.Performances[Context.PerformanceIndex];
		}

		/** The current record goes to the actor with a parameter, and the next becomes current. */
		void Append(FBattleContext& Context, uint8 Actor, uint16 Parameter)
		{
			if (FBattlePerformance* Current = Record(Context))
			{
				Current->Actor = Actor;
				Current->Parameter = Parameter;
				Context.PerformanceIndex = static_cast<uint8>(Context.PerformanceIndex + 1);
			}
		}

		/** A record of a code alone for the actor. */
		void AppendCode(FBattleContext& Context, uint8 Actor, uint8 Code, uint16 Parameter)
		{
			if (FBattlePerformance* Current = Record(Context))
			{
				Current->Code = Code;
				Append(Context, Actor, Parameter);
			}
		}

		void Message(FBattleContext& Context, uint8 Actor, const FEntry& Entry)
		{
			if (Context.NextMessageBox > LastMessageBox)
			{
				return;
			}
			if (Context.Show != nullptr)
			{
				Context.Show->SetMessageBox(Context.NextMessageBox, Entry.Message);
			}
			const uint8 Box = Context.NextMessageBox;
			Context.NextMessageBox = static_cast<uint8>(Box + 1);
			AppendCode(Context, Actor, MessageCode, Box);
		}

		void CloseMessage(FBattleContext& Context, uint8 Actor)
		{
			if (Context.NextMessageBox != 0)
			{
				AppendCode(Context, Actor, CloseMessageCode, static_cast<uint16>(Context.NextMessageBox - 1));
			}
		}

		/** An act's or an effect's message first: shown, held, closed. */
		void ActMessage(FBattleContext& Context, uint8 Actor, const FEntry& Entry)
		{
			if (Entry.Message != 0)
			{
				Message(Context, Actor, Entry);
				AppendCode(Context, Actor, WaitCode, MessageWait);
				CloseMessage(Context, Actor);
			}
		}

		void Act(FBattleContext& Context, uint8 Actor, const FEntry& Entry)
		{
			ActMessage(Context, Actor, Entry);
			Context.CurrentAttack = static_cast<uint8>(Entry.Command + 1);
			PerformBattleCommand(Context, Actor, static_cast<uint8>(Context.CurrentAttack - 1), Entry.Targets, Entry.Code);
			if (Context.Fault != EBattleFault::None || Record(Context) == nullptr)
			{
				return;
			}
			CommitBattleResults(Context, Context.PerformanceIndex);
			FBattlePerformance& Current = Context.Performances[Context.PerformanceIndex];
			Current.Code = Entry.Code;
			Current.Targets = Context.Action.Targets;
			Append(Context, Actor, Entry.Value);
			if (Entry.Targets == Context.Action.Targets)
			{
				return;
			}
			for (uint8 Index = 0; Index < Context.PerformanceIndex; ++Index)
			{
				if (Context.Performances[Index].Code == MoveCode)
				{
					Context.Performances[Index].Targets = Context.Action.Targets;
					return;
				}
			}
		}

		void Move(FBattleContext& Context, uint8 Actor, const FEntry& Entry, uint8 First)
		{
			FBattlePerformance* Current = Record(Context);
			if (Current == nullptr)
			{
				return;
			}
			Current->Code = MoveCode;
			Current->Parameter = 0;
			Current->Targets = Entry.Targets;
			BattleSteps::PlanApproachPath(Context, Actor, First);
			if (static_cast<uint8>(Entry.Value) != 1)
			{
				if (InGear(Context, Actor))
				{
					BattleSteps::MoveGearToTargetGroup(Context, Actor, First);
				}
				else
				{
					BattleSteps::MoveToTargetGroup(Context, Actor, First);
				}
			}
			if (Context.Show != nullptr)
			{
				Context.Show->FrameCamera(static_cast<uint16>(Entry.Targets | SlotBit(Actor)));
			}
			Append(Context, Actor, Entry.Value);
		}

		void Escape(FBattleContext& Context, uint8 Actor, const FEntry& Entry)
		{
			AppendCode(Context, Actor, EscapeCode, Entry.Value);
			Context.Ready[Actor] = NeverAgain;
			Context.Escaped[Actor - FBattleContext::PartySlotCount] = true;
			Context.Combatants[Actor].Status &= 0x8000;
		}

		/** The actor becomes the enemy the entry names: its name, scripts and whole record (the variables and the slot stay). */
		void Transform(FBattleContext& Context, uint8 Actor, const FEntry& Entry)
		{
			const uint8 Into = FirstSlot(Entry.Targets);
			if (Into < FBattleContext::PartySlotCount || Into >= FBattleContext::SlotCount)
			{
				Context.Fault = EBattleFault::SlotOutsideBattle;
				return;
			}
			const std::size_t From = Into - FBattleContext::PartySlotCount;
			const std::size_t To = Actor - FBattleContext::PartySlotCount;
			Context.NameEntries[Actor] = Context.NameEntries[Into];
			Context.AiRecords[To].Scripts = Context.AiRecords[From].Scripts;
			Context.HasHitScript[To] = Context.HasHitScript[From];
			Context.HasReactionScript[To] = Context.HasReactionScript[From];
			Context.Combatants[Actor] = Context.Combatants[Into];
			AppendCode(Context, Actor, TransformCode, Into);
		}

		void BadEntry(FBattleContext& Context)
		{
			for (FBattlePerformance& Performance : Context.Performances)
			{
				Performance.Code = EmptyCode;
			}
		}
	}

	namespace BattleAiSteps
	{
		bool ExecuteEntry(FBattleContext& Context, uint8 Actor, uint8 Index)
		{
			const FEntry Entry = ReadEntry(Context, Index);
			uint8 First = FirstSlot(Entry.Targets);
			if (First >= FBattleContext::SlotCount)
			{
				First = 0;
			}
			if (Record(Context) == nullptr)
			{
				return true;
			}
			ClearBattlePerformance(Context);
			switch (Entry.Type)
			{
			case 0x00: return false;
			case 0x01: Act(Context, Actor, Entry); break;
			case 0x02: Move(Context, Actor, Entry, First); break;
			case 0x03: AppendCode(Context, Actor, StanceCode, Entry.Value); break;
			case 0x04: AppendCode(Context, Actor, static_cast<uint8>(Entry.Value), Entry.Value); break;
			case 0x05: Context.ScriptedTurns = Entry.Targets; break;
			case 0x06: Escape(Context, Actor, Entry); break;
			case 0x07: Transform(Context, Actor, Entry); break;
			case 0x08: SetByteField(Context, Actor, Entry.Command, static_cast<uint8>(Entry.Value)); break;
			case 0x09:
				SetByteField(Context, Actor, Entry.Command, static_cast<uint8>(Entry.Value + ByteField(Context, Actor, Entry.Command)));
				break;
			case 0x0A: SetWordField(Context, Actor, Entry.Command, Entry.Value); break;
			case 0x0B: SetWordField(Context, Actor, Entry.Command, static_cast<uint16>(Entry.Value + WordField(Context, Actor, Entry.Command))); break;
			case 0x0C: Message(Context, Actor, Entry); break;
			case 0x0D:
				ActMessage(Context, Actor, Entry);
				AppendCode(Context, Actor, EffectCode, Entry.Value);
				break;
			case 0x0E: AppendCode(Context, Actor, WaitCode, static_cast<uint8>(Entry.Value)); break;
			case 0x0F:
				if (FBattlePerformance* Current = Record(Context))
				{
					Current->Code = CameraCode;
					Current->Targets = Entry.Targets;
					Append(Context, Actor, Entry.Value);
				}
				break;
			case 0x10: CloseMessage(Context, Actor); break;
			default: BadEntry(Context); break;
			}
			return true;
		}
	}

	void ExecuteBattleAiQueue(FBattleContext& Context, uint8 Actor)
	{
		bool bSawEmpty = false;
		for (uint8 Index = 0; Context.Fault == EBattleFault::None; ++Index)
		{
			if (Index >= EntryCount)
			{
				if (bSawEmpty)
				{
					return;
				}
				Context.Fault = EBattleFault::EntryOutsideQueue;
				return;
			}
			if (!BattleAiSteps::ExecuteEntry(Context, Actor, Index))
			{
				bSawEmpty = true;
			}
		}
	}

	void FinishBattleAiQueue(FBattleContext& Context, uint8 Actor)
	{
		if (InGear(Context, Actor))
		{
			if (FBattlePerformance* Current = Record(Context))
			{
				Current->Actor = Actor;
				Current->Code = GearCloseCode;
				Context.PerformanceIndex = static_cast<uint8>(Context.PerformanceIndex + 1);
			}
		}
		if (FBattlePerformance* Current = Record(Context))
		{
			Current->Actor = Actor;
			Current->Code = EndCode;
		}
		Context.Party.bMenuSounds = false;
	}

	void RecordBattleHit(FBattleContext& Context, uint8 Actor, uint8 Target)
	{
		if (Target < FBattleContext::PartySlotCount)
		{
			return;
		}
		const uint8 Enemy = static_cast<uint8>(Target - FBattleContext::PartySlotCount);
		SetW(Context, Enemy, 7, SlotBit(Actor));
		for (uint8 Byte = 0; Byte < 4; ++Byte)
		{
			SetB(Context, Enemy, static_cast<uint8>(9 + Byte), Context.Action.Info[Byte]);
		}
		SetB(Context, Enemy, 13, Context.Action.InfoCommand);
		const uint16 Fallen = BattleRules::GameSlotBit(Context, Context.SlotTargets[Actor]);
		SetB(Context, Enemy, 14, (Context.Action.KnockedOut & Fallen) != 0 ? 1 : 0);
	}

	void PrepareBattleEnemyActions(FBattleContext& Context, uint8 Slot)
	{
		Context.PerformanceIndex = 0;
		ResetBattleTurnTotals(Context);
		ClearBattlePerformance(Context);
		Context.Performances[0].Actor = Slot;
	}

	void BeginBattleEnemyPlayback(FBattleContext& Context, uint8 Slot)
	{
		if (Context.Show != nullptr)
		{
			Context.Show->BeginPlayback(true, Slot, 0, FindNextReadySlot(Context, Slot));
		}
	}

	void RunBattleEnemyActions(FBattleContext& Context, uint8 Slot)
	{
		ExecuteBattleAiQueue(Context, Slot);
		if (Context.Fault == EBattleFault::None)
		{
			FinishBattleAiQueue(Context, Slot);
		}
	}

	uint8 FindNextReadySlot(const FBattleContext& Context, uint8 Actor)
	{
		int32 Best = 0xFF;
		uint8 Next = 0xFF;
		std::size_t Index = Context.TurnOrderNext;
		do
		{
			const uint8 Slot = Context.TurnOrder[Index];
			if (static_cast<int16>(Context.Waits[Slot]) < Best && Slot != Actor)
			{
				Best = Context.Waits[Slot] & 0xFF;
				Next = Slot;
			}
			Index = Index + 1 == FBattleContext::SlotCount ? 0 : Index + 1;
		} while (Index != Context.TurnOrderNext);
		return Next;
	}
}
