#include "Support/BattlePartyScenario.h"

#include <array>

namespace Weltall::Testing
{
	namespace
	{
		/** The presses' mixes a case draws from, as battle_party_scenario.STYLES. */
		const std::array<std::vector<uint8>, 4> Styles{{
			{0, 1, 2, 3, 0, 1, 2, 3, 4, 4, 4, 6, 6, 7, 7, 5, 8, 8, 0x0C, 0x0D},
			{4, 4, 6, 6, 7, 7, 4, 6, 7, 0, 1, 2, 3, 5, 8},
			{0, 1, 2, 3, 0, 1, 2, 3, 4, 4, 4, 6, 5, 8},
			{0, 4, 4, 4, 6, 7, 5, 0, 1, 2, 3, 8},
		}};
		constexpr std::array<uint8, 5> Runs{0, 1, 2, 3, 8};
		constexpr std::size_t BillyRecord = 0x26C + 4 * 0xA4;
		constexpr std::size_t AmmunitionCounts = 0x2286;
		constexpr std::size_t GearCounts = 0x22B6;
		constexpr std::size_t Gears = 0x978;
		constexpr std::size_t Skills = 0x16C0;
		constexpr std::size_t GameParty = 0x1D34;

		uint8 Byte(FXorShift32& G)
		{
			return static_cast<uint8>(G.Next());
		}

		bool Flag(FXorShift32& G)
		{
			return G.Next() % 2 != 0;
		}

		/** The turn work's bytes the digest takes, in battle_party_scenario.TURN_BYTES's order (booleans 0 or 1). */
		void DrawTurnBytes(FXorShift32& G, FBattlePartyInput& Party)
		{
			Party.ApLeft = Byte(G);
			Party.ApAtStart = Byte(G);
			Party.ButtonCount = Byte(G);
			Party.MenuState = Byte(G);
			Party.bTurnOver = Flag(G);
			Party.Cost = Byte(G);
			Party.bMenuFading = Flag(G);
			Party.bComboClosed = Flag(G);
			Party.ShownState = Byte(G);
			Party.bStoresAp = Flag(G);
			Party.Deathblow = Byte(G);
			Party.bHitEndsCombo = Flag(G);
			Party.Choice = Byte(G);
			Party.GearHits = Byte(G);
			Party.Cursor = Byte(G);
			Party.bTargetLocked = Flag(G);
			Party.bActed = Flag(G);
			Party.bBlockedOnce = Flag(G);
		}

		void DigestTurnBytes(FOracleDigest& Digest, const FBattlePartyInput& Party)
		{
			Digest.U8(Party.ApLeft, Party.ApAtStart, Party.ButtonCount, Party.MenuState, Party.bTurnOver ? 1 : 0, Party.Cost,
				Party.bMenuFading ? 1 : 0, Party.bComboClosed ? 1 : 0, Party.ShownState, Party.bStoresAp ? 1 : 0, Party.Deathblow,
				Party.bHitEndsCombo ? 1 : 0, Party.Choice, Party.GearHits, Party.Cursor, Party.bTargetLocked ? 1 : 0, Party.bActed ? 1 : 0,
				Party.bBlockedOnce ? 1 : 0);
		}

		/** Billy's gun's ammunition kind at Offset of the state, with its count at Counts emptied a third of the time. */
		void DrawAmmunition(FXorShift32& G, FGameState& State, std::size_t Offset, std::size_t Counts)
		{
			auto Block = State.Block();
			Block[Offset] = static_cast<uint8>(G.Next() % 64);
			if (G.Next() % 3 == 0)
			{
				Block[Counts + Block[Offset]] = 0;
			}
		}
	}

	FBattlePartyCase DrawBattlePartyState(FXorShift32& G, FBattleContext& Context, FGameState& State)
	{
		FBattlePartyCase Case;
		Case.Actor = static_cast<uint8>(G.Next() % 3);
		Case.bAuto = G.Next() % 8 == 0;
		for (std::size_t Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
		{
			uint8 InGear = 0;
			if (G.Next() % (Slot == Case.Actor ? 2u : 4u) == 0)
			{
				InGear = static_cast<uint8>(1 + G.Next() % 2);
			}
			Context.ApRecords[Slot].InGear = InGear;
			FBattleCombatant& Member = Context.Combatants[Slot];
			Member.BattleFlags = static_cast<uint8>((Member.BattleFlags & 0x7F) | (InGear != 0 ? 0x80 : 0));
			if (InGear != 0)
			{
				Member.EtherBuffs &= static_cast<uint16>(~0x1000);
			}
		}
		if (Context.ApRecords[Case.Actor].InGear != 0)
		{
			Context.HasHitScript = {};
		}
		Context.InBattle[Case.Actor] = true;
		if (G.Next() % 4 == 0)
		{
			Context.Combatants[Case.Actor].Character = G.Next() % 2 == 0 ? 4 : 7;
		}
		Context.PartyCharacters[Case.Actor] = Context.Combatants[Case.Actor].Character;
		FBattleCombatant& Actor = Context.Combatants[Case.Actor];
		Actor.MaxHp = static_cast<uint16>(G.Next() % 4000);
		Actor.Hp = static_cast<uint16>(G.Next() % (Actor.MaxHp + 1u));
		for (int32 Deathblow = 0; Deathblow < 7; ++Deathblow)
		{
			Context.Commands[FBattleContext::PartyCommandIndex(Case.Actor, 7 + Deathblow)].ApCost = static_cast<uint8>(1 + G.Next() % 6);
		}
		for (const int32 Command : {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 37, 38, 39, 40})
		{
			Context.Commands[FBattleContext::GearCommandIndex(Case.Actor, Command)].FuelCost = static_cast<uint16>(G.Next() % 300);
		}
		for (int32 Ability = 0; Ability < 16; ++Ability)
		{
			Context.Commands[FBattleContext::PartyCommandIndex(Case.Actor, 22 + Ability)].EpCost = static_cast<uint8>(G.Next() % 40);
		}
		Context.PerformanceIndex = 0;
		Context.TurnActor = Case.Actor;
		for (FBattlePerformance& Record : Context.Performances)
		{
			if (Record.Code == 0xFE)
			{
				Record.Code = 0xFF;
			}
		}
		Context.Occupancy[16 + G.Next() % 8].Count = 0;
		for (int32 Route = 0; Route < 64; ++Route)
		{
			uint8 Open = 0;
			if (G.Next() % 4 == 0)
			{
				Open = Byte(G);
			}
			Context.Stage.Route(Route / 8, Route % 8, 0) = Open;
		}
		DrawAmmunition(G, State, BillyRecord + 0x6F, AmmunitionCounts);
		DrawAmmunition(G, State, BillyRecord + 0x72, AmmunitionCounts);
		const uint8 Gear = static_cast<uint8>(G.Next() % 20);
		State.Block()[BillyRecord + 0xA0] = Gear;
		DrawAmmunition(G, State, Gears + Gear * 0xA4u + 4, GearCounts);
		DrawAmmunition(G, State, Gears + Gear * 0xA4u + 7, GearCounts);
		if (G.Next() % 2 == 0)
		{
			const std::size_t GearDeathblows = Skills + Context.Combatants[Case.Actor].Character * 0x20u + 4;
			State.Block()[GearDeathblows] = 0xFF;
			State.Block()[GearDeathblows + 1] = 0xFF;
		}
		for (std::size_t Member = 0; Member < FBattleContext::PartySlotCount; ++Member)
		{
			State.Block()[GameParty + Member] = Context.PartyCharacters[Member];
		}

		FBattlePartyInput& Party = Context.Party;
		DrawTurnBytes(G, Party);
		for (uint8& Deathblow : Party.ComboQueue)
		{
			Deathblow = Byte(G);
		}
		for (uint8& Button : Party.Buttons)
		{
			Button = Byte(G);
		}
		for (auto& Disabled : Party.Disabled)
		{
			for (std::size_t Command = 0; Command < Disabled.size(); ++Command)
			{
				Disabled[Command] = G.Next() % 4 == 0 ? static_cast<uint16>(0x8000u >> Command) : 0;
			}
		}
		const uint32 Previous = G.Next() % 5;
		const uint32 Last = G.Next() % 5;
		Party.PreviousDirection = static_cast<uint8>(Previous == 4 ? 0xFF : Previous);
		Party.LastDirection = static_cast<uint8>(Last == 4 ? 0xFF : Last);
		Party.ListScroll = static_cast<int32>(G.Next() % 64) - 32;
		Party.ListScrollWay = static_cast<uint8>(G.Next() % 5);
		Party.ChosenRow = static_cast<uint8>(G.Next() % 24);
		Party.ChosenCell = static_cast<uint8>(G.Next() % 16);
		for (uint8& Cell : Party.ComboCells)
		{
			Cell = Byte(G);
		}
		FBattleTargeting& Targeting = Context.Targeting;
		const uint32 Target = G.Next() % 12;
		Targeting.Target = static_cast<uint8>(Target == 11 ? 0xFF : Target);
		Targeting.Candidates = static_cast<uint16>(G.Next() & 0x7FF);
		for (uint8& Entry : Targeting.List)
		{
			const uint32 Value = G.Next() % 12;
			Entry = static_cast<uint8>(Value == 11 ? 0xFF : Value);
		}
		Targeting.Count = static_cast<uint8>(G.Next() % 12);
		Party.bApproached = Flag(G);
		Party.bStoredCombo = false;
		Party.bGearPressed = Flag(G);
		Party.bAutoTurn = false;
		Party.bMenuSounds = Flag(G);
		Party.bIconsShown = Flag(G);
		Party.ListWindow = 0;
		if (G.Next() % 4 == 0)
		{
			Party.ListWindow = static_cast<uint8>(G.Next() % 6);
		}
		Party.bAmmunitionShown = G.Next() % 4 == 0;
		Party.TurnIcons = {};
		Party.Event = 8;
		Context.bGearsLocked = Flag(G);
		for (std::size_t Cell = 0; Cell < Context.ListedItems.size(); ++Cell)
		{
			uint8 Item = 0;
			if (G.Next() % 3 != 0)
			{
				Item = static_cast<uint8>(1 + G.Next() % 47);
			}
			Context.ListedItems[Cell] = Item;
			Context.ListedCounts[Cell] = Item != 0 ? static_cast<uint8>(1 + G.Next() % 3) : 0;
		}
		const uint32 Style = G.Next() % (Styles.size() + 1);
		const std::size_t Length = 20 + G.Next() % 140;
		if (Style < Styles.size())
		{
			for (std::size_t Press = 0; Press < Length; ++Press)
			{
				Case.Script.push_back(Styles[Style][G.Next() % Styles[Style].size()]);
			}
		}
		else
		{
			Case.Script = {static_cast<uint8>(G.Next() % 2 != 0 ? 3 : 1), 4};
			while (Case.Script.size() < Length)
			{
				const uint8 Press = Runs[G.Next() % Runs.size()];
				Case.Script.insert(Case.Script.end(), 1 + G.Next() % 10, Press);
			}
			Case.Script.resize(Length);
		}
		Case.PlaybackFrames = static_cast<uint8>(G.Next() % 4);
		Case.bPicksTarget = G.Next() % 4 != 0;
		if (Case.bAuto)
		{
			const uint8 Enemy = static_cast<uint8>(3 + G.Next() % 8);
			Context.InBattle[Enemy] = true;
			Context.Slots[Enemy].Presence = 0;
			Context.Combatants[Enemy].Status &= static_cast<uint16>(~0xC001);
			Context.ApRecords[Enemy].InGear = 0;
			Context.Stage.Route(Context.Slots[Case.Actor].Group, Context.Slots[Enemy].Group, 0) = 0;
		}
		Context.bActionsPlayed = false;
		return Case;
	}

	void DigestBattlePartyState(FOracleDigest& Digest, const FBattleContext& Context)
	{
		const FBattlePartyInput& Party = Context.Party;
		const FBattleTargeting& Targeting = Context.Targeting;
		Digest.Raw(Party.ComboQueue);
		Digest.Raw(Party.Buttons);
		Digest.U8(Context.bActionsPlayed ? 1 : 0);
		DigestTurnBytes(Digest, Party);
		for (const auto& Disabled : Party.Disabled)
		{
			for (const uint16 Flag : Disabled)
			{
				Digest.I16(Flag);
			}
		}
		Digest.U8(Party.PreviousDirection, Party.LastDirection, Party.Event);
		Digest.U8(Targeting.Target);
		Digest.I16(Targeting.Candidates);
		Digest.Raw(Targeting.List);
		Digest.U8(Targeting.Count);
		Digest.I32(static_cast<uint32>(Party.ListScroll));
		Digest.U8(Party.ListScrollWay, Party.ChosenRow, Party.ChosenCell);
		Digest.Raw(Party.ComboCells);
		Digest.U8(Party.bApproached ? 1 : 0, Party.bStoredCombo ? 1 : 0, Party.bGearPressed ? 1 : 0, Party.bAutoTurn ? 1 : 0,
			Party.bMenuSounds ? 1 : 0);
		Digest.U8(Party.bIconsShown ? 1 : 0, Party.ListWindow, Party.bAmmunitionShown ? 1 : 0);
		Digest.Raw(Party.TurnIcons);
		Digest.Raw(Context.ListedItems);
		Digest.Raw(Context.ListedCounts);
	}
}
