#include "Support/BattleProgramDigest.h"

#include "Battle/BattleRuntime.h"
#include "Game/GameState.h"
#include "Support/BattleAiScenario.h"
#include "Support/BattleEventScenario.h"
#include "Support/BattlePartyScenario.h"
#include "Support/BattleScenario.h"

#include <array>

namespace Weltall::Testing
{
	std::vector<uint8> BattlePadButtons()
	{
		std::vector<uint8> Buttons;
		const auto Add = [&Buttons](uint8 Button, int32 Count)
		{
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Buttons.push_back(Button);
			}
		};
		Add(14, 30);
		Add(12, 12);
		Add(15, 12);
		constexpr std::array<uint8, 4> Directions{4, 5, 6, 7};
		constexpr std::array<uint8, 5> Rare{13, 8, 9, 0, 3};
		for (int32 Twice = 0; Twice < 2; ++Twice)
		{
			for (const uint8 Direction : Directions)
			{
				Add(Direction, 1);
			}
		}
		for (const uint8 Button : Rare)
		{
			Add(Button, 1);
		}
		return Buttons;
	}

	void DigestBattleProgramState(FOracleDigest& Digest, const FBattleRuntime& Runtime, const FGameState& Game, const FPsxRandom& Random, bool bStarted,
		std::optional<bool> ListCursorShown)
	{
		const FBattleContext& Context = Runtime.Context();
		DigestBattleState(Digest, Context, Game, Random);
		DigestBattleAiState(Digest, Context, Game);
		DigestBattlePartyState(Digest, Context);
		DigestBattleEventState(Digest, Context, ListCursorShown);
		Digest.U8(Context.Turn.bActorGlow ? 1 : 0);
		Digest.I16(Game.BattleActingTurns);
		Digest.U8(Context.Party.InputMode, Runtime.Input().IsPaused() ? 1 : 0, bStarted ? 1 : 0, Game.ProgramResult);
		for (const FBattleSlot& Slot : Context.Slots)
		{
			Digest.U8(Slot.FacesLeft);
		}
	}

	void DigestBattleResultRun(FOracleDigest& Digest, const FBattleContext& Context)
	{
		const FBattleResultRun& Run = Context.ResultRun;
		if (!Run.bBegun)
		{
			return;
		}
		Digest.U8(Run.bPanelsShown ? 1 : 0, Run.bLevelUpShown ? 1 : 0, Run.bLearnedShown ? 1 : 0, Run.bSpoilsShown ? 1 : 0);
		Digest.U8(Run.bWindowShown[0] ? 1 : 0, Run.bWindowShown[1] ? 1 : 0, Run.bWindowShown[2] ? 1 : 0, Context.Event.bKeyWait ? 1 : 0);
		Digest.U8(Run.bScreenWork ? 1 : 0);
		if (!Run.bScreenWork)
		{
			return;
		}
		Digest.U8(Run.bEtherRows ? 1 : 0, Run.bCounting ? 1 : 0);
		for (std::size_t Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
		{
			Digest.U8(Run.bExpCounted[Slot] ? 1 : 0, Run.bEtherCounted[Slot] ? 1 : 0);
			// An empty slot's counter takes what lies past the characters' records as the count ends: not compared.
			const bool bOccupied = Context.Slots[Slot].Occupant != FBattleSlot::Empty;
			Digest.I32(bOccupied ? Context.StartFigures[Slot].Exp : 0u, bOccupied ? Context.StartFigures[Slot].EtherExp : 0u);
			Digest.I32(Context.Result.ExpGains[Slot], Context.Result.EtherExpGains[Slot]);
		}
		Digest.U8(Run.bFanfarePlayed ? 1 : 0);
	}
}
