#include "Support/GameText.h"

#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

namespace Weltall::Testing
{
	FMesResident LoadMesResident(const FIso9660& Disc)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		auto Resident = Weltall::LoadMesResident(Disc.Image(), *Index, *Executable);
		REQUIRE(Resident);
		return std::move(*Resident);
	}

	FMesNameTables MakeGlyphMapTables(std::initializer_list<FMesCode> Glyphs)
	{
		constexpr uint32 GlyphMapTable = 27;
		FByteWriter File;
		const uint32 MapOffset = 4 + (GlyphMapTable + 1) * 4;
		File.U32(GlyphMapTable + 1);
		for (uint32 Table = 0; Table <= GlyphMapTable; ++Table)
		{
			File.U32(MapOffset);
		}
		for (const FMesCode Glyph : Glyphs)
		{
			File.U8(Glyph.Lead).U8(Glyph.Code);
		}
		auto Tables = FMesNameTables::Parse(File.Bytes);
		REQUIRE(Tables);
		return std::move(*Tables);
	}
}
