// Prints a disc map's scripts and messages as the text of a map of its own over it: what a story document that makes a
// map starts from, and what a patch of the map is written against.
//
//   WeltallMapText MAP [NUMBER] [DISC]    map MAP of disc DISC (1 or 2; 1) as "map NUMBER from MAP" (NUMBER 730 to 1023; 1000)
//
// The discs are the checkout's, in its Games folder.

#include "Field/FieldMap.h"
#include "Field/FieldMapPatch.h"
#include "Media/DiscIdentification.h"
#include "Media/GameFileIndex.h"
#include "Media/GameLibrary.h"
#include "Media/PsxExecutable.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace Weltall;

namespace
{
	int Fail(const std::string& Why)
	{
		std::fprintf(stderr, "%s\n", Why.c_str());
		return 1;
	}
}

int main(int ArgumentCount, char** Arguments)
{
	if (ArgumentCount < 2 || ArgumentCount > 4)
	{
		return Fail("usage: WeltallMapText MAP [NUMBER] [DISC]");
	}
	const int32 Map = std::atoi(Arguments[1]);
	const int32 Number = ArgumentCount > 2 ? std::atoi(Arguments[2]) : 1000;
	const int32 DiscNumber = ArgumentCount > 3 ? std::atoi(Arguments[3]) : 1;
	const FGameLibrary Library = FGameLibrary::Scan(std::filesystem::path(WELTALL_GAMES_DIR));
	const FIso9660* Disc = Library.Find(DiscNumber == 2 ? EDiscRelease::UsaDisc2 : EDiscRelease::UsaDisc1);
	if (Disc == nullptr)
	{
		return Fail("the disc is not in the Games folder");
	}
	const auto Executable = ReadBootExecutable(*Disc);
	if (!Executable)
	{
		return Fail("the disc's program cannot be read");
	}
	const auto Index = FGameFileIndex::Read(*Executable);
	if (!Index)
	{
		return Fail(Index.Error().Message);
	}
	const auto Archive = LoadFieldArchive(Disc->Image(), *Index, Map);
	if (!Archive)
	{
		return Fail(Archive.Error().Message);
	}
	const auto Text = WriteFieldMapText(Number, Map, Archive->Section(EFieldSection::EventScripts), Archive->Section(EFieldSection::Messages));
	if (!Text)
	{
		return Fail(Text.Error().Message);
	}
	std::fputs(Text->c_str(), stdout);
	return 0;
}
