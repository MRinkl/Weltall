#pragma once

#include "Foundation/Types.h"

#include <array>

namespace Weltall
{
	/** What a turn hands the show besides the calls its parts make. */
	enum class EBattleTurnCue : uint8
	{
		/** The action's name rendered from the battle's messages (Arg the message). */
		ActionName,
		/** The action's name box shown (Arg 1) or hidden (Arg 0). */
		ActionNameBox,
		/** Every message box hidden, the name boxes with them. */
		HideMessageBoxes,
		/** The next actor's attack readied (Arg its slot). */
		ReadyNext,
		/** The combatants' idle poses chosen again from their states. */
		RefreshPoses,
	};

	/** A turn under way: where it stopped for the next frame, and what its steps keep between frames. */
	struct FBattleTurnWork
	{
		static constexpr int32 MaxReturns = 4;

		uint8 Pc = 0;
		/** The turn of a slot an enemy's script ran (no turn script). */
		bool bScripted = false;
		/** The enemy the reactions reached. */
		uint8 Reactor = 0;
		/** Frames the step under way still takes. */
		uint8 Frames = 0;
		/** Where the shared steps (the action's message, an enemy's actions, the event's hook) return to. */
		std::array<uint8, MaxReturns> Returns{};
		uint8 Depth = 0;
		/** The enemy whose actions are being carried out. */
		uint8 Performer = 0;
		bool bRunning = false;
		/** The acting party member's panel glows (from its turn's start to the turn's end). */
		bool bActorGlow = false;
	};
}
