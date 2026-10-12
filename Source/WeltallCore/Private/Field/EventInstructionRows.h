#pragma once

#include "Foundation/Types.h"

#include <span>
#include <string_view>

namespace Weltall
{
	/**
	 * A row of the instruction table. Operands lists the operands by a letter each, with spaces between: b a
	 * byte, w a word, h a signed word, l a long, v a value, V a variable, a an actor, s a script, j a jump, m a
	 * message, f the flags byte, and o with two hexadecimal digits an operand by that bit of the flags.
	 */
	struct FEventInstructionRow
	{
		uint16 Opcode = 0;
		std::string_view Name;
		std::string_view Operands;
	};

	/** The one-byte instructions, 00 to FF. */
	std::span<const FEventInstructionRow> BaseEventInstructionRows();

	/** The two-byte instructions FE begins, FE00 on. */
	std::span<const FEventInstructionRow> ExtendedEventInstructionRows();
}
