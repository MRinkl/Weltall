#pragma once

#include "Field/EventContext.h"
#include "Field/FieldRequests.h"
#include "Foundation/Result.h"

#include <array>
#include <optional>

namespace Weltall
{
	/**
	 * The field's main loop's rules between its ticks, on the context alone: what it waits for before
	 * it takes up each request, in its order after the tick (a battle, a map change, then on the ticks
	 * that draw into buffer 1 the world map, the arena, another program), and the player's buttons it
	 * takes up when nothing is asked for (the gears' boarding, the menu).
	 */
	namespace FieldMainLoop
	{
		/** The draw buffer on whose ticks the programs and movies start; the menu opens on the other's. */
		inline constexpr uint8 ProgramBuffer = 1;

		/**
		 * The field is quiet: no song, joining member's sprite file or extra animation set is loading,
		 * and the effect textures' place holds no picture of the screen.
		 */
		WELTALLCORE_API bool IsIdle(const FEventContext& Context);

		/** Start pauses the field before its next tick: in the tick's repeating buttons, Cross not held, the pause not disabled (FE E0). */
		WELTALLCORE_API bool PausesOnStart(const FEventContext& Context);

		/**
		 * The battle asked for starts now. A script's waits for a tick that draws into buffer 1 and a
		 * quiet field; an encounter's, as soon as the field is quiet and the leader stands on the
		 * ground (or the player's action mode is on): the original also waits for the Battle program's
		 * file and the battle's song to load from the disc and for buffer 1, which the port does not.
		 */
		WELTALLCORE_API bool BattleStarts(const FEventContext& Context);

		/**
		 * The map change asked for goes ahead now: no song, joining member or extra animations loading,
		 * the next map's file read, and the map's fade layer at rest. The first time it finds the file
		 * not read it reads it, and the change waits a tick.
		 */
		WELTALLCORE_API bool MapChangeGoesAhead(FEventContext& Context);

		/** The world map, the arena or another program, in that order, when one is asked for and starts now; None otherwise. */
		WELTALLCORE_API EFieldRequest ProgramStarting(const FEventContext& Context);

		/**
		 * No program, battle, world map or map change is asked for and the field is quiet: the main loop
		 * takes up the gears' boarding, Select's picture, a movie, the menu and the menu's button, in
		 * that order.
		 */
		WELTALLCORE_API bool TakesUpPlayer(const FEventContext& Context);

		/**
		 * The gears can be neither boarded nor left now: the party is not in gears or the leader stands
		 * on a floor that forbids it, unless the scripts decide it (FE B7).
		 */
		WELTALLCORE_API bool IsGearBoardingLocked(const FEventContext& Context);

		/**
		 * L2 and R2 held together, while the player controls the leader, board or leave the gears (once
		 * a press: bButtonsHeld keeps what the last look saw): the kernel's lock worked out again, then
		 * the leader on the ground, no menu asked for, and the boarding not locked. Fills Slots with the
		 * party slots that go with the first slot's member (riding as it rides, or walking as it walks)
		 * and whose characters have gears; false otherwise.
		 */
		WELTALLCORE_API bool TakeGearBoarding(FEventContext& Context, bool& bButtonsHeld, std::array<bool, 3>& Slots);

		/** Select asks for the map's picture: in the tick's repeating buttons, no map change asked for, the player in control. */
		WELTALLCORE_API bool SelectShowsPicture(const FEventContext& Context);

		/**
		 * Select's picture of the map the map word names, when the map has one and the party holds the
		 * map's item: the picture, the parts the game state's flags hide, and where the leader's marker
		 * goes. None otherwise.
		 */
		WELTALLCORE_API std::optional<FFieldMapPicture> MapPicture(const FEventContext& Context);

		/** The movie asked for plays now: on a tick that draws into buffer 1. */
		WELTALLCORE_API bool MoviePlays(const FEventContext& Context);

		/** The menu asked for opens now: on a tick that draws into buffer 0, the leader on the ground. */
		WELTALLCORE_API bool MenuOpens(const FEventContext& Context);

		/** The player's menu button asks for the Normal Menu, with the parameter FE 99 set: the menu not locked, none asked for, the player in control. */
		WELTALLCORE_API bool TakeMenuButton(FEventContext& Context);

		/** The menu the player's button asked for stays shut when it would open: the scripts have locked the menu since. */
		WELTALLCORE_API bool MenuStaysShut(const FEventContext& Context);

		/**
		 * What the menu's end hands the scripts after Load Game, by the menu's result (0 New Game, 1
		 * the title timed out, 2 a game was loaded): after New Game map 4 from its entrance 0, after a
		 * load the loaded game's map (and entrance, for a field map), in the variables and the game
		 * state; either way the per-tick update leaves the game state alone from then on.
		 */
		WELTALLCORE_API void TakeLoadGameResult(FEventContext& Context, uint8 MenuMode, uint8 ModeResult);

		/**
		 * The party slots whose riding the menu changed (Riding: each slot's riding as the menu opened),
		 * whose gears the field then boards or leaves: in gear mode, for the slots with an actor.
		 */
		WELTALLCORE_API std::array<bool, 3> GearsChangedByMenu(const FEventContext& Context, const std::array<uint8, 3>& Riding);

		/**
		 * The field leaves for the program Request names, as its main loop leaves: the gears' boarding
		 * lock for the next program, the effect textures given back, the per-tick update once more, every
		 * particle effect ended, the positional sounds stopped, the windows closed, the party's sprite
		 * files to be read again; then the game state takes what the program change hands on (for a
		 * battle the field's variables and the song it comes back to, for a battle or the arena the
		 * entrance). Request's program, battle and flags are filled in. An error when the leader has
		 * no script state.
		 */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> LeaveField(FEventContext& Context, FFieldRequest& Request);
	}
}
