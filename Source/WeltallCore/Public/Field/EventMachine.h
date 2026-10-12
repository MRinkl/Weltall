#pragma once

#include "Field/EventContext.h"
#include "Foundation/Result.h"

#include <optional>

namespace Weltall
{
	/**
	 * The field's event interpreter. Every tick each script actor, in index order, runs its most
	 * urgent thread for up to 8 instructions (fewer when one ends its turn, more when instructions
	 * grant them); a thread that waits leaves its pc on the waiting instruction. It keeps no state of
	 * its own: everything is in the context.
	 */
	class FEventMachine
	{
	public:
		static constexpr int32 TickBudget = 8;
		static constexpr int32 DefaultScript = 1;
		static constexpr uint32 DefaultPriority = 7;
		static constexpr int32 TalkScript = 2;
		static constexpr int32 TouchScript = 3;
		static constexpr uint32 TalkPriority = 3;
		static constexpr uint32 TouchPriority = 4;

		/**
		 * A fresh arrival on the map: marks the actors that have nothing to say, then runs each
		 * actor's init script (script 0) to its end, in actor order; an actor whose init created no
		 * model or sprite gets the map's first sprite set. Returns the fault that stopped the scripts.
		 */
		[[nodiscard]] WELTALLCORE_API static std::optional<FError> InitMap(FEventContext& Context);

		/** What a map's start sets of the variables: 0x010 cleared, the party's characters in 0x03E, 0x040 and 0x042. */
		WELTALLCORE_API static void SetMapStartVariables(FEventContext& Context);

		/** One tick of events; stops early when a script asks for a battle, the world map or another map. */
		[[nodiscard]] WELTALLCORE_API static std::optional<FError> Update(FEventContext& Context);

		/**
		 * Runs the first actor's script Script at once to its end, as the map's init runs one, with its
		 * threads cleared, and gives the actor back its state afterwards (what the script did to the
		 * rest stays): the field's pass after the menu (script 3). Returns the fault that stopped it.
		 */
		[[nodiscard]] WELTALLCORE_API static std::optional<FError> RunScriptAside(FEventContext& Context, int32 Script);

		/**
		 * Starts a script of an actor in its first free slot, unless one of its threads already runs
		 * that script; as the talk and touch checks start scripts 2 and 3. False when nothing started.
		 */
		WELTALLCORE_API static bool StartScript(FEventContext& Context, int32 Actor, int32 Script, uint32 Priority);
	};
}
