#include "Chd/ChdMetadata.h"

#include "Chd/ChdBytes.h"

#include <array>

namespace Chd
{
	namespace
	{
		constexpr uint32 EntryHeaderBytes = 16;
		/** More entries than any disc has: a chain that goes on past this loops. */
		constexpr std::size_t MostEntries = 4096;
	}

	TResult<std::vector<FMetadata>> ReadMetadata(const IInput& Input, const FHeader& Header)
	{
		std::vector<FMetadata> Entries;
		for (uint64 Offset = Header.MetadataOffset; Offset != 0;)
		{
			if (Entries.size() == MostEntries)
			{
				return EError::Corrupt;
			}
			std::array<uint8, EntryHeaderBytes> Raw{};
			if (Offset > Input.Size() || Input.Size() - Offset < EntryHeaderBytes)
			{
				return EError::CutShort;
			}
			if (!Input.Read(Offset, Raw))
			{
				return EError::CannotRead;
			}
			FMetadata& Entry = Entries.emplace_back();
			Entry.Tag = BigEndian::ReadU32(Raw, 0);
			Entry.Flags = Raw[4];
			const uint32 Length = BigEndian::ReadU24(Raw, 5);
			if (Input.Size() - Offset - EntryHeaderBytes < Length)
			{
				return EError::CutShort;
			}
			Entry.Data.resize(Length);
			if (!Input.Read(Offset + EntryHeaderBytes, Entry.Data))
			{
				return EError::CannotRead;
			}
			Offset = BigEndian::ReadU64(Raw, 8);
		}
		return Entries;
	}
}
