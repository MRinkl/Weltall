#include "Game/CharacterEquipment.h"

#include "Foundation/LittleEndian.h"
#include "Foundation/Sha1.h"
#include "Game/GameState.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Support/XorShift32.h"

#include <catch_amalgamated.hpp>

#include <vector>

using namespace Weltall;

namespace
{
	constexpr std::size_t CharactersOffset = 0x26C;
	constexpr std::size_t CharacterSize = 0xA4;
	constexpr std::size_t IdOffset = 0x56;
	constexpr uint8 Billy = 4;

	std::vector<uint8> RandomBytes(Testing::FXorShift32& Random, std::size_t Size)
	{
		std::vector<uint8> Bytes(Size);
		for (std::size_t Offset = 0; Offset < Size; Offset += 4)
		{
			LittleEndian::WriteU32(Bytes, Offset, Random.Next());
		}
		return Bytes;
	}

	std::span<uint8> Record(FGameState& State, int32 Character)
	{
		return State.Block().subspan(CharactersOffset + static_cast<std::size_t>(Character) * CharacterSize, CharacterSize);
	}
}

// The Menu's own recomputation of a character's equipment and stats on random tables and records
// under the oracle harness.
TEST_CASE("A character's equipment and stats are computed as the Menu computes them", "[Game][Menu]")
{
	Testing::FXorShift32 Random(0x57A7E5ED);
	std::vector<uint8> Weapons = RandomBytes(Random, 256 * FEquipmentTables::RecordSize);
	for (std::size_t Weapon = 0; Weapon < 256; ++Weapon)
	{
		if (Random.Next() % 4 == 0)
		{
			Weapons[Weapon * FEquipmentTables::RecordSize + 0x0B] = 100;
		}
	}
	std::vector<uint8> Accessories = RandomBytes(Random, 256 * FEquipmentTables::RecordSize);
	for (std::size_t Accessory = 0; Accessory < 256; ++Accessory)
	{
		Accessories[Accessory * FEquipmentTables::RecordSize + 0x09] = static_cast<uint8>(Random.Next() % 12);
	}
	const FEquipmentTables Tables = FEquipmentTables::Make(std::move(Weapons), std::move(Accessories));
	FSha1 Digest;
	FGameState State;
	for (int32 Case = 0; Case < 400; ++Case)
	{
		const std::vector<uint8> Block = RandomBytes(Random, FGameState::BlockSize);
		std::copy(Block.begin(), Block.end(), State.Block().begin());
		const int32 Character = static_cast<int32>(Random.Next() % 11);
		const uint32 Draw = Random.Next();
		Record(State, Character)[IdOffset] = Draw % 3 == 0 ? Billy : static_cast<uint8>(Random.Next() % 11);
		RecomputeCharacterEquipment(State, Character, Tables);
		const FCharacterStats Stats = ComputeCharacterStats(State, Character, true);
		Digest.Update(State.Block());
		std::array<uint8, 14> Values{};
		for (std::size_t Stat = 0; Stat < Stats.Values.size(); ++Stat)
		{
			LittleEndian::WriteU16(Values, Stat * 2, Stats.Values[Stat]);
		}
		Digest.Update(Values);
	}
	CHECK(FSha1::ToHex(Digest.Finish()) == "d369d9cf9b2215d3c450dbb88d3c665d6a65d8cb");
}

TEST_CASE("Agility shows at most 16 unless the original's display is asked for, where only more than 20 becomes 16", "[Game][Menu]")
{
	const FEquipmentTables Tables = FEquipmentTables::Make({}, {});
	FGameState State;
	for (const auto& [Agility, Fixed, Original] : std::initializer_list<std::array<uint16, 3>>{{12, 12, 12}, {18, 16, 18}, {20, 16, 20}, {21, 16, 16}})
	{
		Record(State, 0)[0x5A] = static_cast<uint8>(Agility);
		RecomputeCharacterEquipment(State, 0, Tables);
		CHECK(ComputeCharacterStats(State, 0, false).Values[FCharacterStats::Agility] == Fixed);
		CHECK(ComputeCharacterStats(State, 0, true).Values[FCharacterStats::Agility] == Original);
	}
}

TEST_CASE("The disc's equipment tables hold the weapons and the accessories", "[Game][Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Tables = FEquipmentTables::Load(Disc->Image(), *Index);
	REQUIRE(Tables);
	CHECK(Tables->Weapon(99).size() == FEquipmentTables::RecordSize);
	CHECK(Tables->Accessory(199).size() == FEquipmentTables::RecordSize);
	// A new game's Fei and Citan with their equipment give the values the Status screen shows.
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	auto NewGame = LoadNewGameState(Disc->Image(), *Index, Resident.NameTables);
	REQUIRE(NewGame);
	constexpr int32 Fei = 0;
	constexpr int32 Citan = 2;
	RecomputeCharacterEquipment(*NewGame, Fei, *Tables);
	RecomputeCharacterEquipment(*NewGame, Citan, *Tables);
	using FValues = std::array<uint16, FCharacterStats::Count>;
	CHECK(ComputeCharacterStats(*NewGame, Fei, false).Values == FValues{15, 10, 15, 12, 7, 8, 11});
	CHECK(ComputeCharacterStats(*NewGame, Citan, false).Values == FValues{18, 14, 18, 18, 10, 10, 13});
}
