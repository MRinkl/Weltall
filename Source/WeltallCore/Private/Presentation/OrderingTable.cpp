#include "Presentation/OrderingTable.h"

namespace Weltall
{
	FOrderingTable::FOrderingTable(int32 EntryCount)
		: Entries(static_cast<std::size_t>(EntryCount))
	{
	}

	void FOrderingTable::Add(int32 Entry, const FDrawPrimitive& Primitive)
	{
		Entries[static_cast<std::size_t>(Entry)].push_back(Primitive);
	}

	void FOrderingTable::AppendTo(FDrawList& List) const
	{
		for (auto Entry = Entries.rbegin(); Entry != Entries.rend(); ++Entry)
		{
			List.Primitives.insert(List.Primitives.end(), Entry->rbegin(), Entry->rend());
		}
	}

	void FOrderingTable::Clear()
	{
		for (std::vector<FDrawPrimitive>& Entry : Entries)
		{
			Entry.clear();
		}
	}
}
