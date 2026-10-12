#pragma once

#include "Battle/BattleContext.h"
#include "Foundation/Types.h"

namespace Weltall
{
	/** What the Battle program's opening hands its show, in the order it runs. */
	enum class EBattleOpeningCue : uint8
	{
		/** The battle's sound effects registered and the start sounds of the battle's kind played (Arg the kind). */
		StartSounds,
		/** The setup's end: the HUD's panels and labels, the objects' pools, the stage built and its clear colours. */
		SetupFinished,
		/**
		 * The presentation begun once the setup's end check has run (the frame's state, the sprites, the scene's
		 * graphics loaded by the task that runs the intro, the camera's first place), the screen black, faded
		 * back over 40 steps unless an event battle does it itself (Arg 1), the HUD's gauges, and the camera
		 * framing the living.
		 */
		Presentation,
		/** The HUD's message boxes. */
		MessageBoxes,
		/** The start sounds' file released, its effects stopped first (Arg 1; 0 for the kind that played none). */
		StartSoundsReleased,
	};

	/** The input's modes, which the HUD draws by and the input step reads by. */
	namespace BattleInputModes
	{
		/** The result screen. */
		inline constexpr uint8 Result = 0;
		/** The battle's commands. */
		inline constexpr uint8 Commands = 1;
		/** The opening's and the event's messages. */
		inline constexpr uint8 Messages = 2;
	}

	/**
	 * What the Battle program asks of its show beside what the rules hand it: the opening's cues, the frame's
	 * two halves around the input step, whether the battle has started, the input step's HUD counters and
	 * the pause's picture.
	 */
	class IBattleProgramShow : public IBattleShow
	{
	public:
		/**
		 * A battle begins, before anything else: the show plays it from the battle's context, which it reads
		 * (and where the rules read back what the show did, writes) until the battle is over.
		 */
		virtual void BeginBattle(FBattleContext& Context) = 0;

		virtual void PlayOpeningCue(EBattleOpeningCue Cue, uint8 Arg) = 0;

		/**
		 * A frame's show up to the input step: the camera and its view, the 3D objects, the sprites' pending
		 * frames, the tasks' draws and updates, the HUD by the input mode.
		 */
		virtual void BeginFrame() = 0;

		/**
		 * The show holds the frame begun for vertical blanks of its own before the input step (a screen wave that
		 * plays its frames inside the frame's): nothing of the battle steps while they pass, one HoldBlank each.
		 */
		virtual bool IsHolding() const = 0;
		virtual void HoldBlank() = 0;

		/**
		 * The steps the frame begun runs beyond its own: the vertical blanks the frame before overran, at most four.
		 * The input step runs once more for each.
		 */
		virtual int32 CatchUpSteps() const = 0;

		/** The frame last begun drew into the first of the two draw buffers: the battle's last clean-up then takes one frame more. */
		virtual bool DrewFirstBuffer() const = 0;

		/**
		 * The rest of the frame after the input step: the extension asked for and, when the frame is not one
		 * the show runs inside its own wait, the playback's step, an effect's pending load and the objects'
		 * idle wait. True when the frame returns to the rules' code; false while the show still waits inside
		 * a routine of its own (the rules' routine then stays where it is).
		 */
		virtual bool FinishFrame() = 0;

		/**
		 * A frame's start, after one that returned to the rules' code: a call the rules made that takes frames goes on
		 * from its last frame. True once it has returned to the rules' code (or none was under way), which then goes on.
		 */
		virtual bool ResumeRules() = 0;

		/** The battle has started: the intro has ended and the end pose has not ended it. The waits run and actors are picked only then. */
		virtual bool HasStarted() const = 0;

		/** The input step's HUD counters, by the input mode: the turn icon's angle and count, the glow's pulse, the menus' animated textures. */
		virtual void TickHud(uint8 InputMode) = 0;

		/** The pause's picture over the frame with the sound paused (the pause, and the wait for a controller), or taken away. */
		virtual void ShowPause(bool bPaused) = 0;

		/** The show met what it does not play: the battle stops at the end of the frame. */
		virtual bool HasStopped() const = 0;
	};
}
