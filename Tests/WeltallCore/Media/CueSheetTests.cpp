#include "Media/CueSheet.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

TEST_CASE("CUE sheet tracks resolve against the sheet's folder", "[Media]")
{
	const auto Tracks = ParseCueSheet(
		"FILE \"Game (Disc 1).bin\" BINARY\n"
		"  TRACK 01 MODE2/2352\n"
		"    INDEX 01 00:00:00\n"
		"  TRACK 02 AUDIO\n"
		"    INDEX 00 10:00:00\n"
		"    INDEX 01 10:02:05\r\n",
		"Games");
	REQUIRE(Tracks);
	REQUIRE(Tracks->size() == 2);
	CHECK((*Tracks)[0].File == std::filesystem::path("Games") / "Game (Disc 1).bin");
	CHECK((*Tracks)[0].SectorSize == 2352);
	CHECK((*Tracks)[0].StartFrame == 0);
	CHECK_FALSE((*Tracks)[0].IsAudio());
	CHECK((*Tracks)[1].IsAudio());
	CHECK((*Tracks)[1].StartFrame == (10 * 60 + 2) * 75 + 5);
}

TEST_CASE("CUE sheet errors name the line", "[Media]")
{
	const auto Result = ParseCueSheet("FILE a.bin BINARY\nTRACK xx MODE1/2048\n", ".");
	REQUIRE_FALSE(Result);
	CHECK(Result.Error().Message.find("line 2") != std::string::npos);
	CHECK_FALSE(ParseCueSheet("REM nothing here\n", "."));
}
