#include "Foundation/LittleEndian.h"
#include "Support/GameDiscs.h"
#include "Support/WorldMapScenario.h"
#include "WorldMap/WorldMapScriptTables.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <initializer_list>
#include <vector>

using namespace Weltall;

// The scripts of the world map's scripted cutscenes and the ribbons' stops, as the program holds them.

namespace
{
	/** Where the first disc's program keeps scene 17's script, the ribbons' stops and scene 18's script. */
	constexpr std::size_t Scene17At = 0x2AC68;
	constexpr std::size_t StopsAt = 0x2AFCC;
	constexpr std::size_t Scene18At = 0x2B170;
	constexpr std::size_t ProgramSize = 0x2B400;

	void Put(std::vector<uint8>& Program, std::size_t At, std::initializer_list<int32> Halfwords)
	{
		for (const int32 Halfword : Halfwords)
		{
			LittleEndian::WriteU16(Program, At, static_cast<uint16>(Halfword));
			At += 2;
		}
	}

	/** A program of zeros but for two short scripts: a wait and the end; a message, a sound and the end. */
	std::vector<uint8> SmallProgram()
	{
		std::vector<uint8> Program(ProgramSize);
		Put(Program, Scene17At, {WorldMapScriptOp::Wait, 10, WorldMapScriptOp::End, 0});
		Put(Program, Scene18At, {WorldMapScriptOp::SendMessage, 2, 1, 0, WorldMapScriptOp::PlaySound, 49, WorldMapScriptOp::End, 0});
		return Program;
	}
}

TEST_CASE("A cutscene's script is read up to and with its end, each instruction by its operation's length", "[WorldMap]")
{
	std::vector<uint8> Program = SmallProgram();
	Put(Program, StopsAt, {2673, -876, 18618, 64});
	const TResult<FWorldMapScriptTables> Tables = ReadWorldMapScriptTables(Program, EDiscRelease::UsaDisc1);
	REQUIRE(Tables);
	CHECK(Tables->Scene17 == FWorldMapScript{WorldMapScriptOp::Wait, 10, WorldMapScriptOp::End, 0});
	CHECK(Tables->Scene18 == FWorldMapScript{WorldMapScriptOp::SendMessage, 2, 1, 0, WorldMapScriptOp::PlaySound, 49, WorldMapScriptOp::End, 0});
	CHECK(Tables->RibbonStops[0][0][1] == -876);
	CHECK(Tables->RibbonStops[2][4][13] == 0);
}

TEST_CASE("A cutscene's script with an operation the program lacks, or without an end, is refused", "[WorldMap]")
{
	std::vector<uint8> Unknown = SmallProgram();
	Put(Unknown, Scene18At, {WorldMapScriptOp::Count});
	CHECK_FALSE(ReadWorldMapScriptTables(Unknown, EDiscRelease::UsaDisc1));

	// Waits to the program's end.
	std::vector<uint8> Endless = SmallProgram();
	for (std::size_t At = Scene18At; At + 4 <= Endless.size(); At += 4)
	{
		Put(Endless, At, {WorldMapScriptOp::Wait, 1});
	}
	CHECK_FALSE(ReadWorldMapScriptTables(Endless, EDiscRelease::UsaDisc1));

	CHECK_FALSE(ReadWorldMapScriptTables(std::vector<uint8>(Scene17At), EDiscRelease::UsaDisc1));
}

TEST_CASE("Both discs hold the scripts of scenes 17 and 18 and the ribbons' three stops", "[WorldMap][Content]")
{
	for (const EDiscRelease Release : {EDiscRelease::UsaDisc1, EDiscRelease::UsaDisc2})
	{
		REQUIRE_DISC(Disc, Release);
		Testing::FWorldMapOracleFiles Files;
		REQUIRE_FALSE(Files.Read(*Disc, Release));
		const FWorldMapScriptTables& Scripts = Files.Tables->Cutscenes.Scripts;
		CHECK(Scripts.Scene17.size() == 434);
		CHECK(Scripts.Scene18.size() == 102);
		CHECK(Scripts.Scene17.front() == WorldMapScriptOp::Wait);
		CHECK(Scripts.Scene17[1] == 10);
		CHECK(Scripts.Scene18[1] == 60);
		// The Ethos' headquarters, Lahan and Nisan, the five ribbons a little apart at each.
		CHECK(Scripts.RibbonStops[0][0] == std::array<int16, 14>{2673, -876, 18618, 64, 128, 255, 2, 1, 0, -64, 0, 0, 20, 0});
		CHECK(Scripts.RibbonStops[1][4] == std::array<int16, 14>{28280, -1248, 10520, 64, 128, 255, 2, 1, 0, -512, 1536, 0, 16, 0});
		CHECK(Scripts.RibbonStops[2][2] == std::array<int16, 14>{20344, -632, 6602, 128, 64, 255, 8, 8, 0, 0, 2048, 0, 24, 0});
	}
}
