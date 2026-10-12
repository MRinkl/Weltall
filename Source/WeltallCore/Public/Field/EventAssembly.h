#pragma once

#include "Field/EventScripts.h"
#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Weltall
{
	/** A script's place in the bytecode: an actor's entry point. */
	struct FEventEntryPoint
	{
		int32 Actor = 0;
		int32 Script = 0;
		uint16 Offset = 0;
	};

	/**
	 * What a text of event scripts assembles to. The text, a line each:
	 *
	 *   actors 14                         the script actors, each with 32 entry points
	 *   unsigned 014 0F0-0FE              variables (their byte addresses, hexadecimal) read as unsigned
	 *   entrances                         the entrance table's mark, the bytecode's first byte
	 *   entrance x 12 z -30 layer 0 camera 2 facing keep
	 *   entry 3 9 @0x1F00                 an entry point (actor, script) that names an offset, not a line
	 *   define Progress var[000]          a name for one operand, from here on
	 *   actor 3 script 2:                 the lines from here are actor 3's script 2
	 *   Again:                            a label
	 *       If Progress >= #16 else Again
	 *       Message msg4 0
	 *       data 01 9B 05                 bytes as they are, hexadecimal
	 *
	 * An instruction is its name and its operands in order. A byte is a number; a word and a long are numbers
	 * (0x for hexadecimal); a value is #n (0 to 32767) or var[address]; a variable is var[address]; an actor
	 * is actorN, party0 to party2 or self; a script is scriptN@priority; a jump is a label or @offset; a
	 * message is msgN; an operand by a flag is #n (signed) or var[address], which sets its bit of the flags
	 * byte, never written itself, and any other bit of that byte is given last as flags=0xNN. If reads
	 * "If a test b else label" (EventTestName's tests). A ';' begins a comment.
	 */
	struct FEventAssembly
	{
		/** The actor count, where the text gives one. */
		std::optional<int32> Actors;
		/** The addresses of the variables the text marks unsigned. */
		std::vector<uint16> UnsignedVariables;
		std::vector<FEventEntryPoint> EntryPoints;
		/** Where the code's first byte lies in the bytecode. */
		uint16 Origin = 0;
		std::vector<uint8> Code;
		/** The text's labels and their offsets in the bytecode. */
		std::vector<std::pair<std::string, uint16>> Labels;

		WELTALLCORE_API std::optional<uint16> Label(std::string_view Name) const;
	};

	struct FEventAssemblyContext
	{
		uint16 Origin = 0;
		/** Names the text may use as if it had defined them: each stands for one operand. */
		std::span<const std::pair<std::string, std::string>> Definitions;
	};

	/** Assembles Text; the error names the line. */
	WELTALLCORE_API TResult<FEventAssembly> AssembleEventText(std::string_view Text, const FEventAssemblyContext& Context = {});

	/** A map's whole script section from an assembly at origin 0 that gives its actor count. */
	WELTALLCORE_API TResult<std::vector<uint8>> BuildEventScriptSection(const FEventAssembly& Assembly);

	/** Text's whole script section: the two steps above. */
	WELTALLCORE_API TResult<std::vector<uint8>> AssembleEventScripts(std::string_view Text);

	/** What following a map's scripts from their entry points finds. */
	struct FEventThreads
	{
		/** The offsets of the instructions a thread can reach, ascending. */
		std::vector<uint16> Reached;
		/** Where a thread meets what is no instruction, leaves the bytecode or lands inside an instruction. */
		std::vector<std::string> Problems;
	};

	/** Follows the threads from every actor's entry points, or from Actor's alone. */
	WELTALLCORE_API FEventThreads FollowEventThreads(const FEventScripts& Scripts, std::optional<int32> Actor = std::nullopt);

	/**
	 * A map's scripts as text that assembles back to the same section, byte for byte: the instructions the
	 * threads reach, the bytes between them as instructions where they read as such (marked unreached) and
	 * as data where they do not, each line with its offset as a comment.
	 */
	WELTALLCORE_API std::string DisassembleEventScripts(const FEventScripts& Scripts);
}
