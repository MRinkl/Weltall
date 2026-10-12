#pragma once

#include "Foundation/Result.h"
#include "Game/GameState.h"
#include "Game/GameStateNames.h"
#include "Game/PsxRandom.h"
#include "Input/KernelPad.h"
#include "Media/GameFileIndex.h"
#include "Menu/MenuProgram.h"
#include "Menu/MenuResources.h"
#include "Text/MesResident.h"
#include "Timing/PlayClock.h"

#include <memory>

namespace Weltall
{
	class FCdImage;
	class FPsxExecutable;

	/**
	 * The menu as a new game opens it from a disc, on its own: the game's state, random numbers, pad,
	 * play clock and text as the kernel keeps them, the menu's files, and the program. The sound is the
	 * caller's.
	 */
	class FMenuPlay
	{
	public:
		/**
		 * The menu asked for by Request over a new game on a disc (its kernel the executable), Disc the
		 * disc's number (1 or 2), its system effects to Sound (none: not heard), the player's settings to
		 * Settings (none: the original's menus), its keys to Keys (none: no Controls page), the title's Exit to
		 * Quit (none: no Exit orb), the party in its player's order (bArrangesParty: FMenuServices's). The disc,
		 * the sound, the settings and the keys must outlive the play.
		 */
		WELTALLCORE_API static TResult<std::unique_ptr<FMenuPlay>> NewGame(const FCdImage& Image, const FPsxExecutable& Executable, int32 Disc,
			const FMenuRequest& Request, IMenuSound* Sound, IMenuSettings* Settings = nullptr, IMenuKeys* Keys = nullptr,
			std::function<void()> Quit = nullptr, bool bArrangesParty = false);

		FMenuPlay(const FMenuPlay&) = delete;
		FMenuPlay& operator=(const FMenuPlay&) = delete;
		WELTALLCORE_API ~FMenuPlay();

		/** The menu's next frame and its vertical blank with Held down; false once the menu has ended. */
		[[nodiscard]] WELTALLCORE_API TResult<bool> Frame(FPadState Held);

		const FMenuProgram& Menu() const { return *Program; }
		const FGameState& Game() const { return GameState; }
		/** The game the menu opens over, to change before its first frame. */
		FGameState& Game() { return GameState; }

	private:
		FMenuPlay(FGameFileIndex InIndex, FMesResident InText, FMenuResources InResources, FGameState InGame);

		FGameFileIndex Index;
		FMesResident Text;
		FMenuResources Resources;
		FGameState GameState;
		FGameStateNames Names;
		FPsxRandom Random;
		FKernelPad Pad;
		FPlayClock Clock;
		std::unique_ptr<FMenuProgram> Program;
	};
}
