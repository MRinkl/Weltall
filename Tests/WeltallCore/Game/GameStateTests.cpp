#include "Game/GameState.h"

#include "Foundation/LittleEndian.h"
#include "Foundation/Sha1.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Text/MessageTable.h"

#include <catch_amalgamated.hpp>

#include <vector>

using namespace Weltall;

namespace
{
	/** Glyph 0 is 'F', 1 'e', 2 'i' and 3 a two-byte code. */
	FMesNameTables MakeNameTables()
	{
		return Testing::MakeGlyphMapTables({{0x00, 0x25}, {0x00, 0x41}, {0x00, 0x45}, {0x81, 0x40}});
	}

	std::vector<uint8> MakeNewGameFile()
	{
		std::vector<uint8> File(FGameState::BlockSize, 0);
		auto SetName = [&File](int32 Name, std::initializer_list<uint16> Glyphs)
		{
			std::size_t Offset = static_cast<std::size_t>(Name) * FGameState::NameBytes;
			for (const uint16 Glyph : Glyphs)
			{
				LittleEndian::WriteU16(File, Offset, Glyph);
				Offset += 2;
			}
		};
		for (int32 Name = 0; Name < FGameState::NameCount; ++Name)
		{
			SetName(Name, {0x000F});
		}
		SetName(0, {0, 1, 2, 0x000F});
		SetName(1, {3, 3, 3, 3, 3, 3, 3, 3, 3, 3});
		SetName(2, {3, 0, 0x000F, 1, 1});
		File[0x1D34] = 0;
		File[0x1D35] = 2;
		File[0x1D36] = FGameState::EmptySlot;
		LittleEndian::WriteU32(File, 0x1924, 100);
		LittleEndian::WriteU16(File, 0x1930 + 2 * 3, 0x1234);
		return File;
	}

	std::vector<uint8> Bytes(std::span<const uint8> Span)
	{
		return {Span.begin(), Span.end()};
	}
}

TEST_CASE("A new game's names are turned from glyph indices into the message encoding", "[Game]")
{
	const auto State = FGameState::NewGame(MakeNewGameFile(), MakeNameTables());
	REQUIRE(State);
	CHECK(Bytes(State->Name(0)) == std::vector<uint8>{0x25, 0x41, 0x45});
	// Ten two-byte glyphs fill the slot, and the terminator is lost.
	CHECK(State->Name(1).size() == FGameState::NameBytes);
	CHECK(State->Name(1)[0] == 0x81);
	CHECK(State->Name(1)[1] == 0x40);
	// The glyphs after the end marker are left out, and the slot's tail is cleared.
	CHECK(Bytes(State->Name(2)) == std::vector<uint8>{0x81, 0x40, 0x25});
	CHECK(State->Block()[2 * FGameState::NameBytes + 6] == 0);
	CHECK(State->Name(3).empty());
}

TEST_CASE("The game state reads the party, gold and saved event variables from its block", "[Game]")
{
	auto State = FGameState::NewGame(MakeNewGameFile(), MakeNameTables());
	REQUIRE(State);
	CHECK(State->Party() == std::array<uint8, 3>{0, 2, FGameState::EmptySlot});
	CHECK(State->Gold() == 100);
	CHECK(State->SavedEventVariable(3) == 0x1234);
	State->SetSavedEventVariable(0, 42);
	CHECK(State->SavedEventVariable(0) == 42);
	CHECK(State->Block()[0x1930] == 42);
	CHECK(State->BattleGlobals() == std::array<uint16, FGameState::BattleGlobalCount>{});
}

TEST_CASE("A new-game file that is too short, or names a glyph the map lacks, is refused", "[Game]")
{
	CHECK_FALSE(FGameState::NewGame(std::vector<uint8>(100, 0), MakeNameTables()));
	std::vector<uint8> File = MakeNewGameFile();
	LittleEndian::WriteU16(File, 0, 4);
	CHECK_FALSE(FGameState::NewGame(File, MakeNameTables()));
}

TEST_CASE("The disc's new game matches the state the kernel loads", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto State = LoadNewGameState(Disc->Image(), *Index, Resident.NameTables);
	REQUIRE(State);
	// The kernel's own loader run on the same file leaves this block.
	CHECK(FSha1::ToHex(FSha1::Hash(State->Block())) == "2ac9c2b075bf83a1f5d9417f8ae8a8238b6ee9ca");
	CHECK(Bytes(State->Name(0)) == std::vector<uint8>{0x25, 0x41, 0x45});
	CHECK(State->Party() == std::array<uint8, 3>{0, 2, FGameState::EmptySlot});
	CHECK(State->Gold() == 100);
	CHECK(State->SavedEventVariable(0) == 0);
}
