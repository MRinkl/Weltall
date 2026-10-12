#pragma once

#include "GearMachine.h"

namespace Weltall::GearMachine
{
	/** A script run's registers: the model it runs (a switch can change it), where it is, what the tick's channels did. */
	struct FGearScriptRun
	{
		/** The model the run began with, which the switch names models from. */
		int32 FirstSlot = 0;
		int32 Slot = 0;
		/** The instruction running, its byte operand, and where the run goes on after it. */
		FGearCodeRef At;
		uint8 B = 0;
		FGearCodeRef Next;
		/** The channels' events word of the tick (-1 at a start and after instructions that change them). */
		int32 Events = 0;
		int32 Ticks = 0;
		/** The run stops; the model's place is StopAt, where it goes on next frame. */
		bool bStop = false;
		FGearCodeRef StopAt;
		/** The run ends without keeping its place: another start or a free took the model's place over. */
		bool bEnd = false;

		void Stop() { Stop(At); }
		void Stop(const FGearCodeRef& Where)
		{
			bStop = true;
			StopAt = Where;
		}
		/** The instruction's word n after its opcode word. */
		FGearCodeRef Word(int32 Index) const { return At + 2 * Index; }
	};

	using FGearOp = void (*)(FGearModels& Models, FGearScriptRun& Run, FGearModel& Model);

	/** The handlers by opcode; a null handler (0 and 71h up) stops the script where it stands. */
	const std::array<FGearOp, 0x71>& GearOps();

	// The handlers' groups, each filling its opcodes in the table (GearScriptOps*.cpp).
	void AddChannelOps(std::array<FGearOp, 0x71>& Ops);
	void AddFlowOps(std::array<FGearOp, 0x71>& Ops);
	void AddMotionOps(std::array<FGearOp, 0x71>& Ops);
	void AddModelOps(std::array<FGearOp, 0x71>& Ops);
}
