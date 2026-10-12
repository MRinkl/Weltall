#include "Support/BattleResultScenario.h"

#include "Foundation/LittleEndian.h"

#include <array>
#include <cstddef>

namespace Weltall::Testing
{
	namespace
	{
		constexpr std::size_t Characters = 11;
		constexpr std::size_t Story = 0x22B6;
		constexpr std::size_t BartGear = 0x26C + 3 * 0xA4 + 0xA0;
		constexpr std::size_t ItemIds = 0x2026;
		constexpr std::array<uint8, 6> Ends = {0, 1, 1, 0x40, 0x81, 0x21};
		constexpr std::array<uint8, 5> Modes = {0, 0, 1, 2, 3};
		constexpr std::array<int8, 4> Penalties = {1, 2, 3, -1};

		uint16 Sparse16(FXorShift32& G)
		{
			const uint32 A = G.Next();
			return static_cast<uint16>(A & G.Next());
		}
	}

	FBattleResultCase DrawBattleResultState(FXorShift32& G, FBattleContext& Context, FGameState& State)
	{
		std::span<uint8, FGameState::BlockSize> Block = State.Block();
		const uint16 Normal = G.Next() % 8 != 0 ? uint16{0x8000} : uint16{0};
		const uint16 Late = G.Next() % 2 != 0 ? uint16{0x4000} : uint16{0};
		LittleEndian::WriteU16(Block, Story, static_cast<uint16>((LittleEndian::ReadU16(Block, Story) & 0x3FFF) | Normal | Late));
		for (std::size_t Character = 0; Character < Characters; ++Character)
		{
			const std::size_t Record = 0x26C + Character * 0xA4;
			Block[Record + 0x56] = G.Next() % 8 != 0 ? static_cast<uint8>(Character) : static_cast<uint8>(G.Next() % Characters);
			for (const std::size_t Level : {std::size_t{0x62}, std::size_t{0x63}})
			{
				Block[Record + Level] = G.Next() % 8 == 0 ? uint8{99} : static_cast<uint8>(1 + G.Next() % 98);
			}
			LittleEndian::WriteU32(Block, Record + 0x44, 1 + G.Next() % 2000);
			LittleEndian::WriteU32(Block, Record + 0x48, 1 + G.Next() % 2000);
			LittleEndian::WriteU16(Block, Record + 0x4E, static_cast<uint16>(G.Next() % 1000));
			LittleEndian::WriteU16(Block, Record + 0x52, static_cast<uint16>(G.Next() % 300));
			for (const std::size_t Offset : {0x58, 0x59, 0x5E, 0x5F, 0x5B, 0x5C})
			{
				Block[Record + Offset] = static_cast<uint8>(G.Next() % 201);
			}
			const std::size_t Skills = 0x16C0 + Character * 0x20;
			for (const std::size_t Word : {0, 2, 4, 6})
			{
				LittleEndian::WriteU16(Block, Skills + Word, Sparse16(G));
			}
			Block[Skills + 0x17] = static_cast<uint8>(G.Next() % 9);
		}
		if (G.Next() % 4 == 0)
		{
			Block[BartGear] = 0x12;
		}
		for (std::size_t Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
		{
			FBattleCombatant& Member = Context.Combatants[Slot];
			uint16 Status = Member.Status & 0x3FFF;
			if (G.Next() % 4 == 0)
			{
				Status |= 0x8000;
			}
			if (G.Next() % 8 == 0)
			{
				Status |= 0x4000;
			}
			Member.Status = Status;
			Member.ExpShare = static_cast<uint8>(G.Next() % 7);
			Member.EtherExpShare = static_cast<uint8>(G.Next() % 7);
			const bool bTop = G.Next() % 3 == 0;
			for (uint16& Counter : Member.DeathblowCounters)
			{
				Counter = bTop ? uint16{0xFFFF} : static_cast<uint16>(G.Next());
			}
		}
		for (std::size_t Slot = FBattleContext::PartySlotCount; Slot < FBattleContext::SlotCount; ++Slot)
		{
			FBattleCombatant& Foe = Context.Combatants[Slot];
			uint16 Status = Foe.Status & 0x7FFF;
			if (G.Next() % 2 == 0)
			{
				Status |= 0x8000;
			}
			Foe.Status = Status;
			Foe.ExpReward = G.Next() % 3000;
			Foe.GoldReward = static_cast<uint16>(G.Next());
			for (std::size_t Index = 0; Index < 2; ++Index)
			{
				Foe.DropChances[Index] = static_cast<uint8>(G.Next() % 101);
				Foe.Drops[Index] = static_cast<uint8>(G.Next() % 64);
				Foe.DropKinds[Index] = static_cast<uint8>(G.Next() % 6);
			}
		}
		FBattleResultCase Case;
		Context.bSpecialParty = G.Next() % 8 == 0;
		Context.Formation.Flags = G.Next() % 4 == 0 ? uint8{8} : uint8{0};
		Context.Event.Requests.bEndedBattle = G.Next() % 16 == 0;
		Context.Event.Requests.bNoResults = G.Next() % 8 == 0;
		Context.End = Ends[G.Next() % Ends.size()];
		Case.ModeResult = Modes[G.Next() % Modes.size()];
		Context.Result.ExpPenalty = 0;
		if (G.Next() % 4 == 0)
		{
			Context.Result.ExpPenalty = Penalties[G.Next() % 4];
		}
		for (std::size_t Listed = 0; Listed < FBattleContext::ListedItemCount; ++Listed)
		{
			Context.ListedItemsAtStart[Listed] = G.Next() % 3 == 0 ? Block[ItemIds + G.Next() % 150] : uint8{0};
			Context.ListedCounts[Listed] = static_cast<uint8>(G.Next() % 100);
		}
		return Case;
	}

	void DigestBattleResultState(FOracleDigest& Digest, const FBattleContext& Context)
	{
		const FBattleResultWork& Result = Context.Result;
		Digest.I32(Result.TotalExp);
		Digest.U8(static_cast<uint8>(Result.ExpPenalty));
		Digest.I16(Result.Defeated);
		for (std::size_t Slot = 0; Slot < 3; ++Slot)
		{
			Digest.I32(Result.ExpGains[Slot], Result.EtherExpGains[Slot]);
		}
		Digest.Raw(Result.DropKinds);
		Digest.Raw(Result.DropIds);
		for (const FBattleDropGroup& Group : Result.DropGroups)
		{
			Digest.U8(Group.Id);
		}
		for (const FBattleDropGroup& Group : Result.DropGroups)
		{
			Digest.U8(Group.Count);
		}
		for (const FBattleDropGroup& Group : Result.DropGroups)
		{
			Digest.U8(Group.Kind);
		}
		Digest.Raw(Result.LearnedDeathblows);
		Digest.Raw(Result.LearnedAbilities);
		for (const auto& Levels : Result.StartLevels)
		{
			Digest.Raw(Levels);
		}
		for (const auto& Levels : Result.LevelsGained)
		{
			Digest.Raw(Levels);
		}
		for (const auto& Stats : Result.NewStats)
		{
			Digest.Raw(Stats);
		}
	}
}
