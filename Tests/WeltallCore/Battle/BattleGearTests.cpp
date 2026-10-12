#include "Battle/BattleAction.h"
#include "Battle/BattleContext.h"
#include "Battle/BattleTables.h"
#include "Battle/BattleTurn.h"
#include "Battle/BattleWaits.h"
#include "Game/GameState.h"
#include "Game/PsxRandom.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <memory>
#include <utility>

using namespace Weltall;

namespace
{
	/** Fei in his gear (slot 0) and an enemy (slot 3), both in the battle. */
	struct FGearDuel
	{
		FGameState State;
		FPsxRandom Random{7};
		std::unique_ptr<FBattleContext> Context = std::make_unique<FBattleContext>();

		FGearDuel()
		{
			Context->GameState = &State;
			Context->Random = &Random;
			for (const std::size_t Slot : {std::size_t{0}, std::size_t{3}})
			{
				Context->InBattle[Slot] = true;
				FBattleCombatant& Combatant = Context->Combatants[Slot];
				Combatant.MaxHp = 500;
				Combatant.Hp = 500;
				Combatant.Hit = 90;
				Combatant.GearPart.MaxHp = 5000;
				Combatant.GearPart.Hp = 5000;
				Combatant.GearPart.Output = 20;
				Combatant.GearPart.AttackMultiplier = 3;
				Combatant.GearPart.Defense = 10;
			}
			Context->Combatants[0].BattleFlags = BattleFlags::InGear;
		}

		FBattleCommand& GearCommand(int32 Number) { return Context->Commands[FBattleContext::GearCommandIndex(0, Number)]; }
	};
}

TEST_CASE("A gear's plain attack on someone on foot uses the on-foot form of the attack", "[Battle]")
{
	FGearDuel Duel;
	FBattleContext& Context = *Duel.Context;
	// Record 0 would only clear the armor break; record 15, its form against someone on foot, hits.
	Duel.GearCommand(0).Handler = 2;
	FBattleCommand& OnFoot = Duel.GearCommand(15);
	OnFoot.Power = 20;
	OnFoot.HitBonus = 60;
	OnFoot.Flags = BattleCommandFlags::NeverMisses;
	PerformBattleCommand(Context, 0, 0, 1u << 3, 0);
	REQUIRE(Context.Fault == EBattleFault::None);
	CHECK(Context.Action.CommandIndex == FBattleContext::GearCommandIndex(0, 15));
	CHECK(Context.Action.Results[3] == BattleResults::HpDamage);
	CHECK(Context.Action.Amounts[3] > 0);
}

TEST_CASE("A gear's turn start shows its costs, Charge and chances, and runs Infinity down", "[Battle]")
{
	FGearDuel Duel;
	FBattleContext& Context = *Duel.Context;
	FBattleCombatant& Fei = Context.Combatants[0];
	for (int32 Number = 0; Number < 15; ++Number)
	{
		Duel.GearCommand(Number).FuelCost = static_cast<uint16>(10 * (Number + 1));
	}
	Fei.GearPart.ChargeMultiplier = 4;
	Fei.GearPart.MaxFuel = 800;
	Fei.GearPart.Fuel = 50;

	SECTION("at attack level 2")
	{
		Fei.AttackLevel = 2;
		PrepareGearTurn(Context, 0);
		CHECK(Context.GearTurn.Costs[14] == 150);
		CHECK(Context.GearTurn.Charge == 4 * 10 + 2 * 20);
		CHECK(Context.GearTurn.ShownAttack == 20 * 3);
		CHECK(Context.GearTurn.Lamps == 0x800);
		CHECK(Context.GearTurn.Level == 2);
		CHECK(Context.GearTurn.InfinityChance == 0);
	}
	SECTION("in Infinity, whose last turn ends it")
	{
		Fei.AttackLevel = 4;
		Fei.Durations[6] = 1;
		Fei.GearPart.Buffs = 0x4000;
		PrepareGearTurn(Context, 0);
		CHECK(Context.GearTurn.Charge == 4 * 10 * 10);
		CHECK(Context.GearTurn.Level == 4);
		CHECK(Fei.AttackLevel == 0);
		CHECK(Fei.GearPart.Buffs == 0);
	}
	SECTION("the Xenogears always shows 99")
	{
		Fei.Gear = 15;
		PrepareGearTurn(Context, 0);
		CHECK(Context.GearTurn.InfinityChance == 99);
	}
}

TEST_CASE("An enemy in a gear in the last three slots cannot mark ether records for the whole party", "[Battle]")
{
	for (const uint8 Slot : {uint8{7}, uint8{8}})
	{
		CAPTURE(Slot);
		FGearDuel Duel;
		FBattleContext& Context = *Duel.Context;
		FBattleCombatant& Enemy = Context.Combatants[Slot];
		Enemy = Context.Combatants[3];
		Context.InBattle[Slot] = true;
		Enemy.BattleFlags = BattleFlags::InGear;
		Enemy.GearPart.Buffs = 0x2000;
		PerformBattleCommand(Context, Slot, 0, 1u << 0, 0);
		// The game would take slot 8's records from past the tables, over the battle's own state.
		CHECK(Context.Fault == (Slot == 8 ? EBattleFault::CommandOutsideTable : EBattleFault::None));
	}
}

TEST_CASE("The commands on disc name the handlers of their tables, the enemies' gear commands the gears' own", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Tables = FBattleTables::Load(Disc->Image(), *Index);
	REQUIRE(Tables);
	// Eight handlers on foot, eleven in gears.
	for (int32 Character = 0; Character < FBattleTables::CharacterCount; ++Character)
	{
		for (const FBattleCommand& Command : Tables->CharacterCommands(Character))
		{
			CHECK(Command.Handler < 8);
		}
	}
	// Three records past the gears' commands hold stray bytes: gear 17's 31 and gear 18's 34 and 35.
	for (int32 Gear = 0; Gear < FBattleTables::GearCount; ++Gear)
	{
		const auto& Commands = Tables->GearCommands(Gear);
		for (std::size_t Number = 0; Number < Commands.size(); ++Number)
		{
			CAPTURE(Gear, Number);
			const bool bStray = (Gear == 17 && Number == 31) || (Gear == 18 && (Number == 34 || Number == 35));
			CHECK((Commands[Number].Handler < 11) != bStray);
		}
	}
	// An enemy's command runs through either table; five use the gears' status cure and fixed damage (8, 9),
	// which the on-foot table does not have, so only an enemy in a gear can use them.
	const auto& Enemies = Tables->EnemyCommands();
	for (std::size_t Number = 0; Number < Enemies.size(); ++Number)
	{
		CAPTURE(Number);
		const bool bGearOnly = Number == 31 || Number == 85 || (Number >= 91 && Number <= 93);
		CHECK(Enemies[Number].Handler < (bGearOnly ? 11 : 8));
		CHECK((Enemies[Number].Handler >= 8) == bGearOnly);
	}
}

TEST_CASE("An undamaged gear's Infinity chance is a tenth lost's, or in the original its party slot's plus one", "[Battle]")
{
	// Fei at level 60 in slot 2, his gear at full HP, the story past the flag that opens Infinity (0x22B6 bit 14).
	const auto Chance = [](const FBattleRuleOptions& Options)
	{
		FGearDuel Duel;
		FBattleContext& Context = *Duel.Context;
		Context.RuleOptions = Options;
		Context.Combatants[2] = Context.Combatants[0];
		Context.Combatants[2].Level = 60;
		Context.Combatants[2].InfinityCounter = 5;
		Context.InBattle[2] = true;
		Duel.State.Block()[0x22B7] = 0x40;
		PrepareGearTurn(Context, 2);
		REQUIRE(Context.Fault == EBattleFault::None);
		return Context.GearTurn.InfinityChance;
	};
	CHECK(Chance(FBattleRuleOptions()) == 1 * (5 + 5));
	CHECK(Chance(FBattleRuleOptions::Original()) == 3 * (5 + 5));
}

TEST_CASE("A gear's element change ends with its count, or in the original clears a status instead", "[Battle]")
{
	// Fei's gear: a guard element for one more turn, and Stop Eth Engine for ten.
	const auto Tick = [](const FBattleRuleOptions& Options)
	{
		FGearDuel Duel;
		FBattleContext& Context = *Duel.Context;
		Context.RuleOptions = Options;
		FBattleCombatant& Fei = Context.Combatants[0];
		Fei.GearPart.Elements = 0x0100;
		Fei.GearPart.Status = 0x0100;
		Fei.Durations[8] = 1;
		Fei.Durations[2] = 10;
		TickStatusDurations(Context, 0);
		REQUIRE(Context.Fault == EBattleFault::None);
		return std::pair{Fei.GearPart.Elements, Fei.GearPart.Status};
	};
	CHECK(Tick(FBattleRuleOptions()) == std::pair<uint16, uint16>{0, 0x0100});
	CHECK(Tick(FBattleRuleOptions::Original()) == std::pair<uint16, uint16>{0x0100, 0});
}

TEST_CASE("A gear stops being hit for sure when its Stop ends, or in the original never", "[Battle]")
{
	const auto Mark = [](const FBattleRuleOptions& Options)
	{
		FGearDuel Duel;
		FBattleContext& Context = *Duel.Context;
		Context.RuleOptions = Options;
		Context.bWaitsRunning = true;
		FBattleCombatant& Fei = Context.Combatants[0];
		Fei.Status = 0x2000;
		Fei.Durations[0] = 1;
		Fei.GearPart.Status = 0x400;
		TickBattleWaits(Context);
		REQUIRE(Context.Fault == EBattleFault::None);
		CHECK(Fei.Status == 0);
		return Fei.GearPart.Status;
	};
	CHECK(Mark(FBattleRuleOptions()) == 0);
	CHECK(Mark(FBattleRuleOptions::Original()) == 0x400);
}

TEST_CASE("PWR loss takes twice the Output off a gear's attack, or in the original does nothing", "[Battle]")
{
	// Fei's gear hits the enemy's gear with a sure attack, from the same draws each time.
	const auto Damage = [](const FBattleRuleOptions& Options, uint16 Status)
	{
		FGearDuel Duel;
		FBattleContext& Context = *Duel.Context;
		Context.RuleOptions = Options;
		Context.Combatants[3].BattleFlags = BattleFlags::InGear;
		Context.Combatants[0].GearPart.Status = Status;
		FBattleCommand& Attack = Duel.GearCommand(1);
		Attack.Power = 20;
		Attack.HitBonus = 60;
		Attack.Flags = BattleCommandFlags::NeverMisses;
		PerformBattleCommand(Context, 0, 1, 1u << 3, 0);
		REQUIRE(Context.Fault == EBattleFault::None);
		REQUIRE(Context.Action.Results[3] == BattleResults::HpDamage);
		return Context.Action.Amounts[3];
	};
	const uint32 Plain = Damage(FBattleRuleOptions(), 0);
	CHECK(Damage(FBattleRuleOptions(), 0x0004) < Plain);
	CHECK(Damage(FBattleRuleOptions::Original(), 0x0004) == Plain);
}

TEST_CASE("A gear's HP takes an amount over 65,535 whole, or in the original its low 16 bits", "[Battle]")
{
	// Chu-Chu's giant form at 300,000 HP under "HP - 1", then 70,000 healed from 100,000.
	const auto HpAfter = [](const FBattleRuleOptions& Options, uint32 Hp, uint8 Result, uint32 Amount)
	{
		FGearDuel Duel;
		FBattleContext& Context = *Duel.Context;
		Context.RuleOptions = Options;
		Context.Slots[0].InGear = 1;
		FBattleGearPart& Gear = Context.Combatants[0].GearPart;
		Gear.MaxHp = 400000;
		Gear.Hp = Hp;
		Context.Action.Results.fill(BattleResults::NoResult);
		Context.Action.Results[0] = Result;
		Context.Action.Amounts[0] = Amount;
		CommitBattleResults(Context, 0);
		return Gear.Hp;
	};
	CHECK(HpAfter(FBattleRuleOptions(), 300000, BattleResults::HpDamage, 299999) == 1);
	CHECK(HpAfter(FBattleRuleOptions::Original(), 300000, BattleResults::HpDamage, 299999) == 300000 - (299999 & 0xFFFF));
	CHECK(HpAfter(FBattleRuleOptions(), 100000, BattleResults::HpHeal, 70000) == 170000);
	CHECK(HpAfter(FBattleRuleOptions::Original(), 100000, BattleResults::HpHeal, 70000) == 100000 + (70000 & 0xFFFF));
}

TEST_CASE("The record keeps a turn's totals whole for the show where the amounts are applied whole", "[Battle]")
{
	const auto Record = [](const FBattleRuleOptions& Options)
	{
		FGearDuel Duel;
		FBattleContext& Context = *Duel.Context;
		Context.RuleOptions = Options;
		Context.Slots[0].InGear = 1;
		Context.Combatants[0].GearPart.MaxHp = 400000;
		Context.Combatants[0].GearPart.Hp = 300000;
		ResetBattleTurnTotals(Context);
		Context.Action.Results.fill(BattleResults::NoResult);
		Context.Action.Results[0] = BattleResults::HpDamage;
		Context.Action.Amounts[0] = 199999;
		CommitBattleResults(Context, 0);
		return Context.Performances[0];
	};
	const FBattlePerformance Whole = Record(FBattleRuleOptions());
	CHECK(Whole.bShownWhole);
	CHECK(Whole.WholeTotals[0] == 199999u);
	CHECK(Whole.WholeTotalKinds[0] == BattleResults::HpDamage);
	CHECK(Whole.Totals[0] == (199999 & 0xFFFF));
	CHECK_FALSE(Record(FBattleRuleOptions::Original()).bShownWhole);
}
