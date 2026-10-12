#include "Field/EventContext.h"

#include "Game/PsxRandom.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr uint16 PlayerLocked = 0xFFFF;

		/** Whether the field lets the leader's steps count toward an encounter this tick. */
		bool MayRoll(const FEventContext& Context)
		{
			if (Context.bBattleRequested || Context.Requests.bWorldMap || Context.Requests.bMapChange)
			{
				return false;
			}
			if (Context.Songs != nullptr && Context.Songs->IsLoading())
			{
				return false;
			}
			return Context.Encounters.Period != 0 && Context.PlayerLock != PlayerLocked && Context.bFieldRunning;
		}

		/**
		 * A formation by the weights: a draw from 0 to their sum, then the last formation with a weight
		 * whose share starts below the draw. A draw of 0 picks none.
		 */
		int32 DrawFormation(const std::array<uint8, FFieldEncounters::FormationCount>& Weights, FPsxRandom& Random)
		{
			std::array<int32, FFieldEncounters::FormationCount> Below{};
			int32 Total = 0;
			for (std::size_t Index = 0; Index < Weights.size(); ++Index)
			{
				Below[Index] = Total;
				Total += Weights[Index];
			}
			const int32 Draw = (Random.Next() * (Total + 1)) >> 15;
			for (int32 Index = FFieldEncounters::FormationCount - 1; Index >= 0; --Index)
			{
				if (Weights[static_cast<std::size_t>(Index)] != 0 && Below[static_cast<std::size_t>(Index)] < Draw)
				{
					return Index;
				}
			}
			return -1;
		}
	}

	bool FFieldEncounters::Schedule(FPsxRandom& Random)
	{
		// Enough draws for any schedule the original finishes: it needs few more than PerPeriod.
		constexpr int32 DrawLimit = 1 << 20;
		Countdown = Period;
		if (PerPeriod == 0)
		{
			Period = 0;
			return true;
		}
		Pending.fill(NoEncounter);
		int32 Draws = 0;
		for (int32 Index = 0; Index < PerPeriod; ++Index)
		{
			for (;;)
			{
				if (++Draws > DrawLimit)
				{
					return false;
				}
				const int32 Tick = static_cast<int32>(static_cast<uint32>(static_cast<uint64>(int64{Random.Next()} * (int64{Period} + 1)))) >> 15;
				if (std::find(Pending.begin(), Pending.end(), static_cast<uint16>(Tick)) == Pending.end())
				{
					Pending[static_cast<std::size_t>(Index)] = static_cast<uint16>(Tick);
					break;
				}
			}
		}
		for (int32 Index = 0; Index < PerPeriod; ++Index)
		{
			++Pending[static_cast<std::size_t>(Index)];
		}
		return true;
	}

	bool RollFieldEncounter(FEventContext& Context)
	{
		if (!MayRoll(Context))
		{
			return true;
		}
		FFieldEncounters& Encounters = Context.Encounters;
		if (Context.Random == nullptr)
		{
			return false;
		}
		if (--Encounters.Countdown == 0 && !Encounters.Schedule(*Context.Random))
		{
			return false;
		}
		const int32 Count = Encounters.PerPeriod;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			uint16& Ticks = Encounters.Pending[static_cast<std::size_t>(Index)];
			if (Ticks != FFieldEncounters::NoEncounter)
			{
				--Ticks;
			}
		}
		int32 Due = 0;
		while (Due < Count && Encounters.Pending[static_cast<std::size_t>(Due)] != 0)
		{
			++Due;
		}
		if (Due >= Count)
		{
			return true;
		}
		Encounters.Pending[static_cast<std::size_t>(Due)] = FFieldEncounters::NoEncounter;
		const int32 Formation = DrawFormation(Encounters.Weights, *Context.Random);
		if (Formation < 0)
		{
			return true;
		}
		Encounters.Formation = static_cast<uint8>(Formation);
		Encounters.bBattleDrawn = true;
		Context.bBattleRequested = true;
		return true;
	}
}
