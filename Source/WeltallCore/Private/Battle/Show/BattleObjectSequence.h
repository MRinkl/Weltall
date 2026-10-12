#pragma once

#include "BattleObjectMachine.h"

namespace Weltall::BattleObjectMachine
{
	/**
	 * A sequence run's registers: the object running (op 1F can change it), the object the run began
	 * with, the instruction and where the run goes on after it, the step's track events and ticks, and
	 * how the run ends.
	 */
	struct FSequenceRun
	{
		int32 FirstSlot = 0;
		int32 Slot = 0;
		FGearCodeRef At;
		uint8 B = 0;
		FGearCodeRef Next;
		/** The tracks' events word of the step (-1 at a start and after instructions that change the tracks). */
		int32 Events = 0;
		int32 Ticks = 0;
		/** The run stops; the running object's place is StopAt, where it goes on next step. */
		bool bStop = false;
		FGearCodeRef StopAt;
		/** The run ends without keeping its place (a start or a free took the object's place over). */
		bool bEnd = false;
		/** Op 02 met without a signal: the playback counts a model record at the run's end. */
		bool bCountRecord = false;

		void Stop() { Stop(At); }
		void Stop(const FGearCodeRef& Where)
		{
			bStop = true;
			StopAt = Where;
		}
		/** Word Index after the opcode word. */
		FGearCodeRef Word(int32 Index) const { return At + 2 * Index; }
	};

	using FSequenceOp = void (*)(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object);
	inline constexpr std::size_t SequenceOpCount = 0x76;

	// The handlers' groups, each filling its opcodes in the table.
	void AddSequenceFlowOps(std::array<FSequenceOp, SequenceOpCount>& Ops);
	void AddSequenceMotionOps(std::array<FSequenceOp, SequenceOpCount>& Ops);
	void AddSequenceModelOps(std::array<FSequenceOp, SequenceOpCount>& Ops);
	void AddSequenceWorldOps(std::array<FSequenceOp, SequenceOpCount>& Ops);

	/** The words of an instruction as the handlers read them. */
	uint16 Word(FBattleObjects& Objects, const FSequenceRun& Run, int32 Index);
	int16 SignedWord(FBattleObjects& Objects, const FSequenceRun& Run, int32 Index);
	/** A jump: the instruction's place plus its word Index. */
	FGearCodeRef JumpTarget(FBattleObjects& Objects, const FSequenceRun& Run, int32 Index = 1);
	/** Starts the queued sequences on the object (its queue off), as ops 5B 2 and 3D do; the run ends when any was. */
	void FlushQueue(FBattleObjects& Objects, FSequenceRun& Run, FBattleObject& Object, uint8 Queued);
}
