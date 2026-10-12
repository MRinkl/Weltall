#pragma once

#include "Field/EventContext.h"
#include "Field/FieldCharacters.h"
#include "Field/FieldControls.h"
#include "Foundation/Result.h"
#include "Timing/PlayClock.h"

#include <optional>

namespace Weltall
{
	/** What a field tick takes from the console around it. */
	struct FFieldTickInput
	{
		/** The pad's words of every vertical blank since the last tick, ORed. */
		FFieldPadWords Pad;
		/** The console's play clock as those vertical blanks left it. */
		FPlayClock Clock;
		/** The player's turn of the view (FEventContext::ViewTurn). */
		int16 ViewTurn = 0;
		/** Told of the characters' drawing as the tick draws them, when given. */
		IFieldCharacterDrawing* Drawing = nullptr;
	};

	/**
	 * One frame of the field's logic, the original's frame in its order: the draw buffers swapped, the
	 * tick's buttons and the frame variables; the scripts, each actor's move, the leader's
	 * collisions, the other actors' layers and landings, the talk and touch checks and the
	 * followers; the camera and the actors' turning; the actors' positional sounds; the fade layers;
	 * the models' and the characters' updates, the particles (unless the screen's picture is held) and
	 * the screen wave; the message windows; the VRAM scrollers. What the scripts ask of the field
	 * (another map, a battle, the menu...) is left in the context for the caller. An error names what
	 * stopped the tick: an instruction or a system the port does not run yet.
	 */
	[[nodiscard]] WELTALLCORE_API std::optional<FError> RunFieldFrame(FEventContext& Context, const FFieldTickInput& Input);

	/**
	 * The music's poll that ends each tick: one of the game's random numbers whatever it finds, a song
	 * still loading loaded, and the map's first ticks counted down.
	 */
	[[nodiscard]] WELTALLCORE_API std::optional<FError> PollFieldMusic(FEventContext& Context);

	/** A frame of the field's logic and the music's poll straight after it, as the ticks the field runs outside its main loop are. */
	[[nodiscard]] WELTALLCORE_API std::optional<FError> TickField(FEventContext& Context, const FFieldTickInput& Input);
}
