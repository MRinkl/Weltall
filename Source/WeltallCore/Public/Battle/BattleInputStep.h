#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	class FGameState;
	class FKernelPad;
	class IBattleProgramShow;
	struct FBattleContext;

	/** What the input step works on: the battle, the kernel's pad, the show and the game state (the play time). */
	struct FBattleInputServices
	{
		FBattleContext* Context = nullptr;
		FKernelPad* Pad = nullptr;
		IBattleProgramShow* Show = nullptr;
		FGameState* Game = nullptr;
	};

	/**
	 * The input step of a battle frame, by the input mode. The battle's commands: while the battle runs
	 * the waits tick; the menus' reader walks the kernel's pad queue for the first entry with a button it
	 * knows (one event a frame, its sound, the directions noted; the event Over once the battle has ended
	 * or the actions have played); the HUD's counters; the Ether list's scroll. The messages: the HUD's
	 * counters, the event's Wait counters and its confirm button. The result screen: its confirm button
	 * and the screen's own step. Start pauses a battle that has started (in the result screen, always):
	 * the step then holds its frame, reading the queue as vertical blanks add to it, until Start again.
	 * With no controller the step holds its frame until one is back. Neither counts as play time.
	 */
	class FBattleInputStep
	{
	public:
		/** The frame's step from its start; false when it holds the frame (paused, or waiting for a controller). */
		WELTALLCORE_API bool Run(const FBattleInputServices& Services);

		/**
		 * A catch-up step of the frame, after its own: in the battle's commands the waits tick, the HUD's counters
		 * step and the list scrolls, but the pad is not read again; the messages' and the result screen's steps
		 * run whole once more, reading the pad again (an event not yet taken can be overwritten). False when it
		 * holds the frame.
		 */
		WELTALLCORE_API bool RunCatchUp(const FBattleInputServices& Services);

		/** A held step goes on with what the vertical blanks since queued; false while it still holds the frame. */
		WELTALLCORE_API bool Resume(const FBattleInputServices& Services);

		/** The step holds its frame. */
		bool IsHolding() const { return Pc != EPc::Idle; }
		/** Start has paused the battle. */
		bool IsPaused() const { return bPaused; }
		/** No controller: the step waits for one. */
		bool IsWaitingForPad() const { return bPadWaitShown; }

	private:
		enum class EPc : uint8
		{
			Idle,
			/** The controller's wait, then the mode's work before its reading. */
			PadWait,
			/** The walk of the pad queue. */
			Read,
		};

		bool Go(const FBattleInputServices& Services);
		/** The mode's walk of the queue; false when it drained the queue paused (the frame holds). */
		bool Walk(const FBattleInputServices& Services);
		void TogglePause(const FBattleInputServices& Services);

		EPc Pc = EPc::Idle;
		/** The input mode the step runs in. */
		uint8 Mode = 0;
		/** The event the menus' reader found so far. */
		uint8 Pending = 0;
		bool bPaused = false;
		bool bPadWaitShown = false;
		/** The play time as the pause or the wait began, given back as it ends. */
		uint32 KeptPlayTime = 0;
	};
}
