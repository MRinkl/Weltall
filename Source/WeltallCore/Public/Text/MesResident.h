#pragma once

#include "Foundation/Result.h"
#include "Text/MesFont.h"
#include "Text/MessageTable.h"

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	class FPsxExecutable;

	/** What the message system keeps for the whole game: the font and the kernel's name tables. */
	struct FMesResident
	{
		FMesFont Font;
		FMesNameTables NameTables;
	};

	/** Loads them as the game does at boot, the box glyph from the executable. */
	WELTALLCORE_API TResult<FMesResident> LoadMesResident(const FCdImage& Image, const FGameFileIndex& Index, const FPsxExecutable& Executable);
}
