#include "Field/EventAssembly.h"
#include "Field/EventInstructionSet.h"
#include "Field/FieldMap.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <map>
#include <string>

using namespace Weltall;

// Every map's scripts on both discs, written out as text and assembled back: the instruction set reads all of them.

namespace
{
	struct FScriptCensus
	{
		int32 Maps = 0;
		std::size_t Reached = 0;
		/** The maps whose threads leave the instructions, with where. */
		std::map<int32, std::vector<std::string>> Problems;
	};

	/** Maps First to End - 1 of a disc that are maps on it (the second disc holds placeholders for those it never loads). */
	FScriptCensus RoundTrip(const FIso9660& Disc, int32 First, int32 End)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		FScriptCensus Census;
		for (int32 Number = First; Number < End; ++Number)
		{
			INFO("map " << Number);
			const auto Archive = LoadFieldArchive(Disc.Image(), *Index, Number);
			if (!Archive)
			{
				REQUIRE(Archive.Error().Message.find("placeholder") != std::string::npos);
				continue;
			}
			const std::vector<uint8>& Section = Archive->Section(EFieldSection::EventScripts);
			const auto Scripts = FEventScripts::Parse(Section);
			REQUIRE(Scripts);
			FEventThreads Threads = FollowEventThreads(*Scripts);
			++Census.Maps;
			Census.Reached += Threads.Reached.size();
			if (!Threads.Problems.empty())
			{
				Census.Problems.emplace(Number, std::move(Threads.Problems));
			}
			const auto Again = AssembleEventScripts(DisassembleEventScripts(*Scripts));
			if (!Again)
			{
				FAIL(Again.Error().Message);
			}
			// Compared apart from the check's own expansion, which would print both sections whole.
			const bool bSame = *Again == Section;
			CHECK(bSame);
		}
		return Census;
	}

	/** The one map whose scripts run off their end, on both discs: the last instruction of map 489's first script lacks its last operands. */
	const std::map<int32, std::vector<std::string>> KnownProblems = {{489, {"000D: a thread reaches what is no instruction"}}};
}

TEST_CASE("Every map's scripts on the first disc assemble back from their text: maps 0 to 182", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FScriptCensus Census = RoundTrip(*Disc, 0, 183);
	CHECK(Census.Maps == 183);
	CHECK(Census.Problems.empty());
}

TEST_CASE("Every map's scripts on the first disc assemble back from their text: maps 183 to 365", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FScriptCensus Census = RoundTrip(*Disc, 183, 366);
	CHECK(Census.Maps == 183);
	CHECK(Census.Problems.empty());
}

TEST_CASE("Every map's scripts on the first disc assemble back from their text: maps 366 to 547", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FScriptCensus Census = RoundTrip(*Disc, 366, 548);
	CHECK(Census.Maps == 182);
	CHECK(Census.Problems == KnownProblems);
}

TEST_CASE("Every map's scripts on the first disc assemble back from their text: maps 548 to 729", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FScriptCensus Census = RoundTrip(*Disc, 548, FieldMapCount);
	CHECK(Census.Maps == 182);
	CHECK(Census.Problems.empty());
}

TEST_CASE("Every map's scripts on the second disc assemble back from their text", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	const FScriptCensus Census = RoundTrip(*Disc, 0, FieldMapCount);
	CHECK(Census.Maps == 205);
	CHECK(Census.Reached == 206123);
	CHECK(Census.Problems == KnownProblems);
}

TEST_CASE("The threads of the first disc's scripts reach the instructions the scripts are made of", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	std::size_t Reached = 0;
	std::array<bool, 0x200> Used{};
	for (int32 Number = 0; Number < FieldMapCount; ++Number)
	{
		const auto Archive = LoadFieldArchive(Disc->Image(), *Index, Number);
		REQUIRE(Archive);
		const auto Scripts = FEventScripts::Parse(Archive->Section(EFieldSection::EventScripts));
		REQUIRE(Scripts);
		const std::span<const uint8> Code(Scripts->Code(), Scripts->CodeSize());
		for (const uint16 Offset : FollowEventThreads(*Scripts).Reached)
		{
			const uint16 Opcode = ReadEventInstruction(Code, Offset)->Shape->Opcode;
			Used[Opcode > 0xFF ? 0x100 + (Opcode & 0xFF) : Opcode] = true;
			++Reached;
		}
	}
	CHECK(Reached == 572015);
	CHECK(std::count(Used.begin(), Used.end(), true) == 410);
}
