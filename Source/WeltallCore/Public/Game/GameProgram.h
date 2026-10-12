#pragma once

#include "Foundation/Result.h"
#include "Input/PadState.h"

#include <optional>
#include <span>

namespace Weltall
{
	/** The game's programs, by the numbers its kernel runs them by. */
	enum class EGameProgram : uint8
	{
		/** The kernel's debug menu: what runs when a program ends without naming the next. */
		DebugMenu = 0,
		Field = 1,
		Battle = 2,
		WorldMap = 3,
		/** The Battling arena. */
		Battling = 4,
		Menu = 5,
		Movie = 6,
	};

	/** Where a program is: running, ended (the kernel runs the next one), or restarting the console (the pad's soft reset). */
	enum class EGameProgramState : uint8
	{
		Running,
		Ended,
		SoftReset,
	};

	/** The program's name, for messages. */
	WELTALLCORE_API const char* GameProgramName(EGameProgram Program);

	/**
	 * One of the game's programs as the game's loop runs it: frame by frame, each frame lasting some of
	 * the console's vertical blanks, until it ends, leaving in the kernel's globals the program to run
	 * next and what it hands over, or the pad restarts the console.
	 */
	class IGameProgram
	{
	public:
		virtual ~IGameProgram() = default;

		/** The vertical blanks the next frame lasts. */
		virtual int32 VblanksPerFrame() const = 0;

		/**
		 * The next frame, after its vertical blanks with the buttons held at each (the console counts
		 * them first). An error names what the port does not run yet, or what stopped the program.
		 */
		[[nodiscard]] virtual std::optional<FError> Frame(std::span<const FPadState> HeldPerVblank) = 0;

		virtual EGameProgramState State() const = 0;
	};
}
