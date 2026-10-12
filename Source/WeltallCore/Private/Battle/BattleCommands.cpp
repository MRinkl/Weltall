#include "Battle/BattleGameState.h"
#include "Battle/BattleRules.h"
#include "Battle/BattleTurn.h"

namespace Weltall
{
	using namespace BattleRules;

	namespace
	{
		constexpr uint8 ChuChu = 7;
		/** In Infinity, the message that Charge fills the fuel ten times over. */
		constexpr uint8 InfinityLevel = 4;
		constexpr uint8 ChargeInInfinityMessage = 0x3D;
	}

	void DefendInBattle(FBattleContext& Context, uint8 Slot)
	{
		Context.Action.Command = 0;
		FBattleCombatant& Member = Context.Combatants[Slot];
		Member.BattleFlags |= BattleFlags::Defending;
		if (Member.IsInGear() && (Member.GearPart.PermanentBuffs & 0x10) != 0)
		{
			Member.GearPart.Status &= 0xFE4F;
			Member.Status &= 0xEFFF;
		}
		if (Context.GearTurn.Level == InfinityLevel)
		{
			Context.Action.Message = ChargeInInfinityMessage;
		}
	}

	void EndBattleDefence(FBattleContext& Context, uint8 Slot)
	{
		Context.Combatants[Slot].BattleFlags &= static_cast<uint8>(~BattleFlags::Defending);
	}

	bool TryBattleEscape(FBattleContext& Context)
	{
		Context.Action.Command = 0;
		const bool bEscaped = Percent(Context) < 50;
		if (bEscaped)
		{
			WriteBackBattleParty(Context);
		}
		return bEscaped;
	}

	void WriteBackBattleParty(FBattleContext& Context)
	{
		using namespace GameStateLayout;
		FGameState& State = *Context.GameState;
		for (std::size_t Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
		{
			if (Context.PartyCharacters[Slot] == FBattleContext::NoCharacter)
			{
				continue;
			}
			FBattleCombatant& Member = Context.Combatants[Slot];
			if (!BattleGameState::HasCharacter(Member.Character))
			{
				Context.Fault = EBattleFault::OutsideGameState;
				return;
			}
			const std::size_t Record = CharacterOffset(Member.Character);
			if (Member.Character == ChuChu && Member.IsInGear())
			{
				Member.Hp = static_cast<uint16>((Member.GearPart.Hp + 1) / 50);
				if (Member.Hp == 0)
				{
					Member.Hp = 1;
				}
			}
			BattleGameState::SetWord(State, Record + Character::Hp, Member.Hp);
			BattleGameState::SetWord(State, Record + Character::Ep, Member.Ep);
			if (BattleGameState::Word(State, Record + Character::MaxHp) < BattleGameState::Word(State, Record + Character::Hp))
			{
				BattleGameState::SetWord(State, Record + Character::Hp, BattleGameState::Word(State, Record + Character::MaxHp));
			}
			if (BattleGameState::Word(State, Record + Character::MaxEp) < BattleGameState::Word(State, Record + Character::Ep))
			{
				BattleGameState::SetWord(State, Record + Character::Ep, BattleGameState::Word(State, Record + Character::MaxEp));
			}
			for (std::size_t Counter = 0; Counter < Member.DeathblowCounters.size(); ++Counter)
			{
				BattleGameState::SetWord(State, Record + Character::DeathblowCounters + 2 * Counter, Member.DeathblowCounters[Counter]);
			}
			BattleGameState::SetWord(State, Record + Character::ExactKills, Member.ExactKills);
			if ((Member.Status & 0xC000) != 0)
			{
				BattleGameState::SetWord(State, Record + Character::Hp, 1);
			}
			const uint8 GearId = Member.Gear;
			if (GearId < 7 || (GearId > 7 && GearId < 17))
			{
				const std::size_t GearRecord = GearOffset(GearId);
				const uint32 MaxHp = LittleEndian::ReadU32(State.Block(), GearRecord + Gear::MaxHp);
				uint32 Hp = Member.GearPart.Hp;
				if (MaxHp < Hp)
				{
					Hp = MaxHp;
				}
				uint16 Fuel = Member.GearPart.Fuel;
				const uint16 MaxFuel = BattleGameState::Word(State, GearRecord + Gear::MaxFuel);
				if (MaxFuel < Fuel)
				{
					Fuel = MaxFuel;
				}
				if ((Member.GearPart.Status & 0x8000) != 0)
				{
					Hp = MaxHp / 10;
				}
				LittleEndian::WriteU32(State.Block(), GearRecord + Gear::Hp, Hp);
				BattleGameState::SetWord(State, GearRecord + Gear::Fuel, Fuel);
			}
		}
	}
}
