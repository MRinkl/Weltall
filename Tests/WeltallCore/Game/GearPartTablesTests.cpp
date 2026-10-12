#include "Game/GearPartTables.h"

#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

#include <vector>

using namespace Weltall;

namespace
{
	constexpr std::size_t PilotsOffset = 0x24808;

	/** An LZSS stream of literals only: groups of a clear flag byte and eight bytes, the last group padded with zeros. */
	std::vector<uint8> PackLiterals(std::vector<uint8> Bytes)
	{
		Bytes.resize((Bytes.size() + 7) / 8 * 8, 0);
		Testing::FByteWriter Stream;
		Stream.U32(static_cast<uint32>(Bytes.size()));
		for (std::size_t Index = 0; Index < Bytes.size(); ++Index)
		{
			if (Index % 8 == 0)
			{
				Stream.U8(0);
			}
			Stream.U8(Bytes[Index]);
		}
		return Stream.Bytes;
	}

	/** A data file of 20 sections, 17-20 holding two frames, one armor, one engine and three parts; the others empty. */
	std::vector<uint8> MakeGameDataFile()
	{
		std::vector<std::vector<uint8>> Sections(20, PackLiterals({}));
		std::vector<uint8> Frames(2 * FGearPartTables::FrameSize, 0x11);
		Frames[FGearPartTables::FrameSize] = 0x22;
		Sections[16] = PackLiterals(Frames);
		Sections[17] = PackLiterals(std::vector<uint8>(FGearPartTables::ArmorSize, 0x33));
		Sections[18] = PackLiterals(std::vector<uint8>(FGearPartTables::EngineSize + 3, 0x44));
		Sections[19] = PackLiterals(std::vector<uint8>(3 * FGearPartTables::PartSize, 0x55));
		Testing::FByteWriter File;
		File.U32(static_cast<uint32>(Sections.size()));
		uint32 Offset = static_cast<uint32>(4 + 4 * Sections.size());
		for (const std::vector<uint8>& Section : Sections)
		{
			File.U32(Offset);
			Offset += static_cast<uint32>(Section.size());
		}
		for (const std::vector<uint8>& Section : Sections)
		{
			File.Bytes.insert(File.Bytes.end(), Section.begin(), Section.end());
		}
		return File.Bytes;
	}

	std::vector<uint8> MakeMenuProgram()
	{
		std::vector<uint8> Program(PilotsOffset + FGearPartTables::GearCount, 0);
		Program[PilotsOffset + 2] = 1;
		Program[PilotsOffset + 19] = 15;
		return Program;
	}
}

TEST_CASE("The gear tables are read from the game's data file and the pilots from the menu program", "[Game]")
{
	const auto Tables = FGearPartTables::Parse(MakeGameDataFile(), MakeMenuProgram());
	REQUIRE(Tables);
	CHECK(Tables->Frame(1).size() == FGearPartTables::FrameSize);
	CHECK(Tables->Frame(1)[0] == 0x22);
	CHECK(Tables->Frame(2).empty());
	CHECK(Tables->Armor(0)[0] == 0x33);
	CHECK(Tables->Armor(1).empty());
	// A record needs all its bytes: the few after the engine table's first make none.
	CHECK_FALSE(Tables->Engine(0).empty());
	CHECK(Tables->Engine(1).empty());
	CHECK(Tables->Part(2)[FGearPartTables::PartSize - 1] == 0x55);
	CHECK(Tables->Part(3).empty());
	CHECK(Tables->Pilot(2) == 1);
	CHECK(Tables->Pilot(19) == 15);
}

TEST_CASE("Gear tables without their sections or pilots are refused", "[Game]")
{
	std::vector<uint8> File = MakeGameDataFile();
	File[0] = 19;
	CHECK_FALSE(FGearPartTables::Parse(File, MakeMenuProgram()));
	std::vector<uint8> Program = MakeMenuProgram();
	Program.pop_back();
	CHECK_FALSE(FGearPartTables::Parse(MakeGameDataFile(), Program));
	Program = MakeMenuProgram();
	Program.back() = 60;
	CHECK_FALSE(FGearPartTables::Parse(MakeGameDataFile(), Program));
}
