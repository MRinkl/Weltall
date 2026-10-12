#include "Battle/BattleCommand.h"
#include "Battle/BattleScene.h"
#include "Battle/BattleTables.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <vector>

using namespace Weltall;

TEST_CASE("A command record's fields are read from their places", "[Battle]")
{
	std::array<uint8, FBattleCommand::RecordSize> Record{};
	for (std::size_t Offset = 0; Offset < Record.size(); ++Offset)
	{
		Record[Offset] = static_cast<uint8>(0x80 + Offset);
	}
	const FBattleCommand Command = FBattleCommand::FromRecord(Record);
	CHECK(Command.Targeting == 0x8180);
	CHECK(Command.Animation == 0x8382);
	CHECK(Command.DamageKind == 0x8988);
	CHECK(Command.Flags == 0x8B8A);
	CHECK(Command.WeaponSlots == 0x90);
	CHECK(Command.Power == 0x91);
	CHECK(Command.EpCost == 0x93);
	CHECK(Command.HitBonus == static_cast<int8>(0x95));
	CHECK(Command.Handler == 0x96);
	CHECK(Command.AttackFormula == 0x9A);
	CHECK(Command.StatusBits == 0x9F9E);
	CHECK(Command.Element == 0xA2);
	CHECK(Command.FuelCost == 0xA5A4);
	CHECK(Command.Speed == static_cast<int8>(0xA7));
}

TEST_CASE("Scenes and stages too short for their records are refused", "[Battle]")
{
	CHECK_FALSE(FBattleScene::Parse(std::vector<uint8>(0x32 + 8 * 0x170 - 1)));
	CHECK_FALSE(FBattleStage::Parse(std::vector<uint8>(0x347)));
	CHECK(FBattleStage::Parse(std::vector<uint8>(0x348)));

	std::vector<uint8> Scene(0x32 + 8 * 0x170, 0);
	Scene[0] = 0xFF;
	Scene[1] = 0xFF;
	CHECK_FALSE(FBattleScene::Parse(Scene));
}

TEST_CASE("The battle's tables hold every character's, gear's and enemy's commands and the battle items", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Tables = FBattleTables::Load(Disc->Image(), *Index);
	REQUIRE(Tables);

	// The basic attacks are the same for everyone: Weak 0-3, Strong 4-5, Fierce 6.
	constexpr std::array<uint8, 7> BasicPowers{5, 5, 6, 7, 10, 12, 20};
	constexpr std::array<int8, 7> BasicHitBonuses{60, 60, 60, 60, 0, 0, -10};
	for (int32 Character = 0; Character < FBattleTables::CharacterCount; ++Character)
	{
		const auto& Commands = Tables->CharacterCommands(Character);
		for (std::size_t Attack = 0; Attack < BasicPowers.size(); ++Attack)
		{
			CHECK(Commands[Attack].Handler == 0);
			CHECK(Commands[Attack].Power == BasicPowers[Attack]);
			CHECK(Commands[Attack].HitBonus == BasicHitBonuses[Attack]);
		}
		// Deathblows 0-6 cost the AP of their combos; Chu-Chu's records say 1.
		constexpr std::array<uint8, 7> DeathblowAp{4, 5, 5, 6, 6, 6, 6};
		for (std::size_t Deathblow = 0; Deathblow < DeathblowAp.size(); ++Deathblow)
		{
			CHECK(Commands[7 + Deathblow].ApCost == (Character == 7 ? 1 : DeathblowAp[Deathblow]));
		}
	}

	// A gear's three attacks cost 10, 20 and 30 fuel, but Chu-Chu's giant form's cost nothing.
	for (int32 Gear = 0; Gear < FBattleTables::GearCount; ++Gear)
	{
		const auto& Commands = Tables->GearCommands(Gear);
		for (std::size_t Attack = 0; Attack < 3; ++Attack)
		{
			CHECK(Commands[Attack].FuelCost == (Gear == 7 ? 0 : 10 * (Attack + 1)));
		}
	}

	// The enemies' mass statuses and their half-HP attack.
	const auto& Enemies = Tables->EnemyCommands();
	CHECK(Enemies[23].Handler == 2);
	CHECK(Enemies[23].StatusGroup == 2);
	CHECK(Enemies[23].StatusBits == 0x1000);
	CHECK(Enemies[23].StatusChance == 100);
	CHECK(Enemies[24].StatusBits == 0x2000);
	CHECK(Enemies[25].Handler == 4);
	CHECK(Enemies[25].Power == 2);

	// Aquasol heals 50 HP, the revives bring back 30 and 100 percent, Stop! stops for 120 steps.
	const auto& Items = Tables->Items();
	CHECK(Items[1].Effect == 0x8000);
	CHECK(Items[1].Power == 1);
	CHECK(Items[10].Effect == 0x100);
	CHECK(Items[10].Power == 3);
	CHECK(Items[11].Power == 10);
	CHECK(Items[39].Effect == 1);
	CHECK(Items[39].Power == 11);
	CHECK(Items[39].Setting == 120);
	CHECK(Items[39].Parameter == 0x2000);
}

TEST_CASE("Every scene's enemies and every stage's layout are read from the disc", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	for (int32 Number = 0; Number < 76; ++Number)
	{
		CAPTURE(Number);
		const auto Scene = FBattleScene::Load(Disc->Image(), *Index, Number);
		REQUIRE(Scene);
		CHECK(Scene->ScriptBlock(0) >= 0x32 + 8 * FBattleCombatant::RecordSize);
	}
	for (int32 Number = 0; Number < 75; ++Number)
	{
		CAPTURE(Number);
		CHECK(FBattleStage::Load(Disc->Image(), *Index, Number));
	}
	CHECK_FALSE(FBattleStage::Load(Disc->Image(), *Index, 75));

	// Scene 0 holds the test records: 50 HP.
	const auto Scene = FBattleScene::Load(Disc->Image(), *Index, 0);
	REQUIRE(Scene);
	CHECK(Scene->Enemy(0).MaxHp == 50);
}
