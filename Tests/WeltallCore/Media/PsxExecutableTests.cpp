#include "Media/PsxExecutable.h"

#include <catch_amalgamated.hpp>

#include <cstring>

using namespace Weltall;

TEST_CASE("PS-X EXE header and RAM-addressed reads", "[Media]")
{
	std::vector<uint8> File(FPsxExecutable::HeaderSize + 8, 0);
	std::memcpy(File.data(), "PS-X EXE", 8);
	const uint32 Fields[] = {0x80010020, 0x80090000, 0x80010000, 8};
	std::memcpy(File.data() + 0x10, Fields, sizeof(Fields));
	const uint8 Body[] = {1, 2, 3, 4, 0x78, 0x56, 0x34, 0x12};
	std::memcpy(File.data() + FPsxExecutable::HeaderSize, Body, sizeof(Body));

	const auto Executable = FPsxExecutable::Parse(File);
	REQUIRE(Executable);
	CHECK(Executable->InitialPc() == 0x80010020);
	CHECK(Executable->InitialGp() == 0x80090000);
	CHECK(Executable->ReadU32(0x80010004) == 0x12345678);
	CHECK(Executable->Read(0x80010006, 4).empty());
	CHECK(Executable->Read(0x8000FFFF, 1).empty());
	CHECK_FALSE(FPsxExecutable::Parse(std::vector<uint8>(16, 0)));
}

TEST_CASE("SYSTEM.CNF names the boot executable", "[Media]")
{
	const auto Path = ParseBootExecutable("BOOT = cdrom:\\SLUS_006.64;1\r\nTCB = 4\r\n");
	REQUIRE(Path);
	CHECK(*Path == "SLUS_006.64");
	CHECK(*ParseBootExecutable("BOOT=cdrom:\\EXE\\MAIN.EXE") == "EXE/MAIN.EXE");
	CHECK_FALSE(ParseBootExecutable("TCB = 4\n"));
}
