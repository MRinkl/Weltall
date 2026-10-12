#include "EventOps.h"

#include "Field/EventMachine.h"

namespace Weltall::EventOps
{
	namespace
	{
		constexpr uint32 ScriptMask = 0x1F;
		constexpr uint32 PriorityShift = 5;
		/** The states of a wait on a started script, in the running thread's word. */
		constexpr uint32 NotWaiting = 0;
		constexpr uint32 WaitingForStart = 1;
		constexpr uint32 WaitingForEnd = 2;

		bool IsScriptRunning(const FEventActorState& State, uint32 Script)
		{
			for (const FEventThread& Thread : State.Threads)
			{
				if (Thread.Script == Script)
				{
					return true;
				}
			}
			return false;
		}

		/** The first slot a new thread may take, or ThreadCount when every slot is busy or reserved. */
		int32 FreeSlot(const FEventActorState& State)
		{
			int32 Slot = 0;
			while (Slot < FEventActorState::ThreadCount && !State.Threads[static_cast<std::size_t>(Slot)].IsAvailable())
			{
				++Slot;
			}
			return Slot;
		}

		/** Puts the operand's script at its priority into the target's slot. */
		void StartIn(FEventContext& Context, FEventThread& Thread, uint32 TargetActor)
		{
			const uint8 Request = CodeByte(Context, 2);
			Thread.Pc = Context.Scripts->EntryPoint(static_cast<int32>(TargetActor), static_cast<int32>(Request & ScriptMask));
			Thread.SetPriority(Request >> PriorityShift);
		}

		/** A script request on a removed actor: nothing starts, and the wait it held is dropped. */
		void DropRequest(FEventContext& Context, FEventActorState& Target)
		{
			FEventActorState& State = Self(Context);
			State.Running().SetRequestWait(NotWaiting);
			Target.Threads[State.RequestSlot].Word &= ~FEventThread::Reserved;
		}

		/**
		 * The target of 07-09, or null when there is none: an empty party slot (the instruction is
		 * skipped) or a fault.
		 */
		FEventActorState* RequestTarget(FEventContext& Context, uint32& Actor)
		{
			Actor = ReadActor(Context, 1);
			if (Actor == FEventContext::NoActor)
			{
				Advance(Context, 3);
				return nullptr;
			}
			return StateOf(Context, Actor);
		}

		/** 07: starts a script of an actor unless it already runs; waits for a free slot. */
		void StartScript(FEventContext& Context)
		{
			uint32 Actor = 0;
			FEventActorState* Target = RequestTarget(Context, Actor);
			if (Target == nullptr)
			{
				return;
			}
			if ((Target->Flags1 & EventActorFlags1::Removed) != 0)
			{
				DropRequest(Context, *Target);
				Advance(Context, 3);
				return;
			}
			const uint32 Script = CodeByte(Context, 2) & ScriptMask;
			if (!IsScriptRunning(*Target, Script))
			{
				const int32 Slot = FreeSlot(*Target);
				if (Slot == FEventActorState::ThreadCount)
				{
					return;
				}
				FEventThread& Thread = Target->Threads[static_cast<std::size_t>(Slot)];
				StartIn(Context, Thread, Actor);
				Thread.Script = static_cast<uint8>(Script);
			}
			Advance(Context, 3);
		}

		/**
		 * The first step of 08 and 09: starts the script and reserves a slot of the target while it
		 * waits. The reservation goes on the slot of the previous request, and the new request slot is
		 * the one the script took: a quirk the waits below then clear on the new slot.
		 */
		void StartAndWait(FEventContext& Context, FEventActorState& Target, uint32 Actor, bool bScriptLast)
		{
			FEventActorState& State = Self(Context);
			const uint32 Script = CodeByte(Context, 2) & ScriptMask;
			if (IsScriptRunning(Target, Script))
			{
				Advance(Context, 3);
				return;
			}
			const int32 Slot = FreeSlot(Target);
			if (Slot == FEventActorState::ThreadCount)
			{
				return;
			}
			FEventThread& Thread = Target.Threads[static_cast<std::size_t>(Slot)];
			StartIn(Context, Thread, Actor);
			Target.Threads[State.RequestSlot].Word |= FEventThread::Reserved;
			if (!bScriptLast)
			{
				Thread.Script = static_cast<uint8>(Script);
			}
			State.RequestSlot = static_cast<uint8>(Slot);
			State.Running().SetRequestWait(WaitingForStart);
			if (bScriptLast)
			{
				Thread.Script = static_cast<uint8>(Script);
			}
		}

		/** Whether the requested script has become the target's running thread or has already ended. */
		bool RequestStarted(const FEventActorState& State, const FEventActorState& Target)
		{
			return Target.Slot == State.RequestSlot || Target.Threads[State.RequestSlot].IsFree();
		}

		/** 08: starts a script of an actor and waits until it has begun to run. */
		void StartScriptWaitStart(FEventContext& Context)
		{
			uint32 Actor = 0;
			FEventActorState* Target = RequestTarget(Context, Actor);
			if (Target == nullptr)
			{
				return;
			}
			if ((Target->Flags1 & EventActorFlags1::Removed) != 0)
			{
				DropRequest(Context, *Target);
				Advance(Context, 3);
				return;
			}
			FEventActorState& State = Self(Context);
			switch (State.Running().RequestWait())
			{
			case NotWaiting:
				StartAndWait(Context, *Target, Actor, false);
				return;
			case WaitingForStart:
				if (!RequestStarted(State, *Target))
				{
					Context.Run.bYield = true;
					return;
				}
				Advance(Context, 3);
				State.Running().SetRequestWait(NotWaiting);
				Target->Threads[State.RequestSlot].Word &= ~FEventThread::Reserved;
				return;
			default:
				return;
			}
		}

		/** 09: starts a script of an actor and waits until it has ended. */
		void StartScriptWaitEnd(FEventContext& Context)
		{
			uint32 Actor = 0;
			FEventActorState* Target = RequestTarget(Context, Actor);
			if (Target == nullptr)
			{
				return;
			}
			if ((Target->Flags1 & EventActorFlags1::Removed) != 0)
			{
				DropRequest(Context, *Target);
				Advance(Context, 3);
				return;
			}
			FEventActorState& State = Self(Context);
			FEventThread& Running = State.Running();
			const uint32 Word = Running.Word;
			switch (Running.RequestWait())
			{
			case NotWaiting:
				StartAndWait(Context, *Target, Actor, true);
				return;
			case WaitingForStart:
				if (RequestStarted(State, *Target))
				{
					// Checks the end at once, without yielding.
					Running.Word = (Word & ~FEventThread::RequestWaitMask) | WaitingForEnd << FEventThread::RequestWaitShift;
					return;
				}
				break;
			case WaitingForEnd:
				if (Target->Threads[State.RequestSlot].IsFree())
				{
					Running.Word = Word & ~FEventThread::RequestWaitMask;
					Target->Threads[State.RequestSlot].Word &= ~FEventThread::Reserved;
					Advance(Context, 3);
					return;
				}
				break;
			default:
				return;
			}
			Context.Run.bYield = true;
		}

		/** 04: ends the thread and sends the actor's default loop back to its start. */
		void EndAndRestartLoop(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			for (FEventThread& Thread : State.Threads)
			{
				if (Thread.Priority() == FEventMachine::DefaultPriority)
				{
					Thread.Pc = Context.Scripts->EntryPoint(Context.Run.Actor, FEventMachine::DefaultScript);
				}
			}
			EndThread(Context);
			Context.Run.bYield = true;
		}

		/** 92: ends every thread of the actor, so its default loop starts again next tick. */
		void ResetThreads(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			for (FEventThread& Thread : State.Threads)
			{
				Thread.WaitCount = 0;
				Thread.Script = FEventThread::NoScript;
				Thread.Pc = 0xFFFF;
				Thread.Word = (Thread.Word & ~(FEventThread::RequestWaitMask | FEventThread::Reserved | FEventThread::MoveStageMask | FEventThread::MoveCounterMask))
					| FEventThread::PriorityMask | FEventThread::MoveCounterMask;
			}
			State.Slot = 0;
			State.RequestSlot = 0;
			Context.Run.bYield = true;
			State.WindowStyle = 0;
			State.SetCallDepth(0);
		}
	}

	void RegisterThreads(FBaseTable& Base, FExtendedTable&)
	{
		Base[0x04] = &EndAndRestartLoop;
		Base[0x07] = &StartScript;
		Base[0x08] = &StartScriptWaitStart;
		Base[0x09] = &StartScriptWaitEnd;
		Base[0x92] = &ResetThreads;
	}
}
