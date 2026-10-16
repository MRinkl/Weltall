#include "Sound/FieldSongTable.h"

#include "Formats/Lzss.h"
#include "Media/GameFileIndex.h"

#include <array>

namespace Weltall
{
	namespace
	{
		/** Where a release's Field program keeps the table: its file in directory 1, the offset once unpacked, the songs. */
		struct FSongTableLayout
		{
			EDiscRelease Release;
			int32 ProgramFile;
			std::size_t Offset;
			std::size_t Songs;
		};

		constexpr int32 ProgramDirectory = 1;
		constexpr std::array<FSongTableLayout, 2> Layouts{{
			{EDiscRelease::UsaDisc1, 14, 0x3E4DC, 73},
			{EDiscRelease::UsaDisc2, 14, 0x3E4DC, 73},
		}};
	}

	TResult<FFieldSongTable> FFieldSongTable::Read(const FCdImage& Image, const FGameFileIndex& Index, EDiscRelease Release)
	{
		const FSongTableLayout* Layout = nullptr;
		for (const FSongTableLayout& Candidate : Layouts)
		{
			if (Candidate.Release == GameplayRelease(Release))
			{
				Layout = &Candidate;
			}
		}
		if (Layout == nullptr)
		{
			return MakeError("no song table is known for this release");
		}
		const FIndexRecord* Record = Index.Find(ProgramDirectory, Layout->ProgramFile);
		if (Record == nullptr)
		{
			return MakeError("the disc lacks the Field program");
		}
		const TResult<std::vector<uint8>> Sectors = ReadFileSectors(Image, *Record);
		if (!Sectors)
		{
			return Sectors.Error();
		}
		const TResult<std::vector<uint8>> Program = UnpackLzss(*Sectors);
		if (!Program)
		{
			return Program.Error();
		}
		if (Program->size() < Layout->Offset + Layout->Songs * 2)
		{
			return MakeError("the Field program is too short to hold its song table");
		}
		FFieldSongTable Table;
		for (std::size_t Song = 0; Song < Layout->Songs; ++Song)
		{
			Table.Songs.push_back({(*Program)[Layout->Offset + Song * 2], (*Program)[Layout->Offset + Song * 2 + 1]});
		}
		return Table;
	}
}
