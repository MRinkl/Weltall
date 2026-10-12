#pragma once

#include "Media/Iso9660.h"
#include "Text/MesResident.h"

#include <initializer_list>

namespace Weltall::Testing
{
	/** The message system's font and name tables from a disc; the test fails if they cannot be read. */
	FMesResident LoadMesResident(const FIso9660& Disc);

	/** Name tables holding nothing but a glyph map (table 27) of these codes, by glyph index. */
	FMesNameTables MakeGlyphMapTables(std::initializer_list<FMesCode> Glyphs);
}
