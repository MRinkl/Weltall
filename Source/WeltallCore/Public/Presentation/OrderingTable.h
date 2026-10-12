#pragma once

#include "Presentation/DrawList.h"

#include <span>
#include <vector>

namespace Weltall
{
	/**
	 * Primitives sorted as a game's 2D ordering table sorts them, for games that draw theirs from the
	 * highest entry down: every entry is drawn after the ones above it, and within an entry the
	 * primitive added last is drawn first (libgpu links each new primitive at the entry's head).
	 */
	class FOrderingTable
	{
	public:
		WELTALLCORE_API explicit FOrderingTable(int32 EntryCount);

		WELTALLCORE_API void Add(int32 Entry, const FDrawPrimitive& Primitive);

		/** Appends everything added, in the order the GPU draws it. */
		WELTALLCORE_API void AppendTo(FDrawList& List) const;

		int32 EntryCount() const { return static_cast<int32>(Entries.size()); }

		/** What was added to an entry, in the order it was added. */
		std::span<const FDrawPrimitive> Entry(int32 Index) const { return Entries[static_cast<std::size_t>(Index)]; }

		/** Empties every entry, keeping the memory for the next tick. */
		WELTALLCORE_API void Clear();

	private:
		std::vector<std::vector<FDrawPrimitive>> Entries;
	};
}
