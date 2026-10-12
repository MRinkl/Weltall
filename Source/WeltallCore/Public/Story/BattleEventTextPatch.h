#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace Weltall
{
	/**
	 * What the port's story changes in a battle event's text, written as text:
	 *
	 *   battle event 19                 the event's script, by the number a formation names
	 *   message 0 from page 2           its message 0 begins at its second page
	 *   message 2 from page 1 after     or after pages of the text's own, told first (MesText.h's text,
	 *   {                               between a line of '{' and a line of '}'), in the window the
	 *   He had caught up.               message was written for
	 *   }
	 *
	 * The disc's words stay the disc's: the text says where a message of it begins, and carries only what it tells
	 * before that. A ';' begins a comment.
	 */
	class FBattleEventTextPatch
	{
	public:
		/** Reads the text; the error names the line. */
		WELTALLCORE_API static TResult<FBattleEventTextPatch> Parse(std::string_view Text);

		int32 Event() const { return EventNumber; }

		/**
		 * The event's message table with the changes made; none where it lacks a message or a page the text names, or
		 * where a message's window is smaller than what is told before it: another table than the one it was written
		 * for, which is left as it is.
		 */
		WELTALLCORE_API std::optional<std::vector<uint8>> Apply(std::span<const uint8> Messages) const;

	private:
		/** A message that begins at a later page of its own, after the text's own pages when it has any. */
		struct FBeginning
		{
			int32 Message = 0;
			int32 Page = 1;
			/** The codes told first, ended by 00; empty without. */
			std::vector<uint8> Before;
		};

		int32 EventNumber = 0;
		std::vector<FBeginning> Beginnings;
	};
}
