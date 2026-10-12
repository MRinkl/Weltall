#pragma once

#include "Game/GameLoop.h"
#include "Game/GameSession.h"
#include "Media/GameFileIndex.h"
#include "Media/Iso9660.h"
#include "Media/PsxExecutable.h"
#include "Sound/GameSoundSession.h"
#include "Sound/UnheardGameSound.h"

#include <functional>
#include <memory>
#include <optional>

namespace Weltall::Testing
{
	/**
	 * The game played from a disc for a test: its sound (kept going as the vertical blanks pass, as the
	 * console's would), the session and the loop, powered on or entering a field map at once.
	 */
	struct FGameLoopSession
	{
		std::unique_ptr<FPsxExecutable> Executable;
		std::unique_ptr<FGameFileIndex> Index;
		/** The file index of the other disc the session was given, if any. */
		std::unique_ptr<FGameFileIndex> OtherIndex;
		std::unique_ptr<FGameSoundSession> Sound;
		std::unique_ptr<FUnheardGameSound> Unheard;
		std::unique_ptr<FGameSession> Session;
		std::unique_ptr<FGameLoop> Loop;
		/** The frames and vertical blanks run so far. */
		int32 Frames = 0;
		int32 Vblanks = 0;

		/** The session on the disc; null with the reason as a test failure. */
		static std::unique_ptr<FGameLoopSession> Start(const FIso9660& Disc);

		/** Gives the session the game's other disc, whose movies it plays while the drive holds it. */
		void AddDisc(const FIso9660& Disc);

		void PowerOn();
		void EnterField(uint16 Map);

		/** One frame of the loop with Held down through its vertical blanks; its error, if it stopped. */
		std::optional<FError> Frame(FPadState Held = {});

		/**
		 * Frames with Held down until Done says so (checked before each), at most Limit; the frames run.
		 * A stop of the loop fails the test unless bStopExpected.
		 */
		int32 RunUntil(const std::function<bool()>& Done, int32 Limit, FPadState Held = {}, bool bStopExpected = false);
	};
}
