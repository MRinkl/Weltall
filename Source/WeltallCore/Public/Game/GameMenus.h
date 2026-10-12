#pragma once

#include "Foundation/Result.h"
#include "Game/GameProgram.h"

#include <memory>

namespace Weltall
{
	class FGameSession;
	struct FFieldRequest;

	/**
	 * Where the programs that run the Menu inside themselves get it: for the field's request (the mode,
	 * its parameter, whether the player's button opened it, the gears' boarding lock), a program run a
	 * menu frame (one vertical blank) at a time until it ends, leaving its result in the game state's
	 * ProgramResult as the Menu does (for Load Game: 0 New Game, 1 the title timed out, 2 a game was
	 * loaded, the loaded game in the game state).
	 */
	class IGameMenus
	{
	public:
		virtual ~IGameMenus() = default;

		/** The menu the field asks for, started from the session's services and the kernel's globals the field set for it. */
		virtual TResult<std::unique_ptr<IGameProgram>> Open(FGameSession& Session, const FFieldRequest& Request) = 0;
	};
}
