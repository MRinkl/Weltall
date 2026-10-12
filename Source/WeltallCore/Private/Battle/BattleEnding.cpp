#include "Battle/BattleEnding.h"

#include "Battle/BattleContext.h"
#include "Battle/BattleEvent.h"
#include "Game/GameState.h"

namespace Weltall
{
	namespace
	{
		enum EEndingPc : uint8
		{
			EndingDone,
			RunAfterBattle,
			LoadsFinishing,
			PoseTaking,
			EventEnding,
		};

		namespace EndKinds
		{
			constexpr uint8 Won = 0;
			constexpr uint8 Escaped = 1;
			constexpr uint8 Lost = 2;
			constexpr uint8 ScriptedDefeat = 3;
			/** What the game's sorting leaves an event battle lost without a scripted defeat when the field started it: none of the kinds. */
			constexpr uint8 Unsorted = 0xFF;
		}

		constexpr uint8 EndedBits = 0xC0;
		constexpr uint8 EscapedBit = 0x40;
		constexpr uint16 AfterBattleStory = 0xFF;
		constexpr std::size_t KnockOutVariables = 0x10;
		constexpr uint16 KnockedOut = 0x8000;

		bool ShowBlocks(const FBattleContext& Context)
		{
			return Context.Show != nullptr && Context.Show->IsBlocking();
		}

		void Cue(FBattleContext& Context, EBattleEndCue Kind, uint8 Arg)
		{
			if (Context.Show != nullptr)
			{
				Context.Show->PlayEndCue(Kind, Arg);
			}
		}

		uint8 SortEnd(FBattleContext& Context)
		{
			const uint8 End = Context.End;
			if ((End & EndedBits) == 0)
			{
				return EndKinds::Won;
			}
			if ((End & EscapedBit) != 0)
			{
				return EndKinds::Escaped;
			}
			if (!Context.bEventBattle)
			{
				return EndKinds::Lost;
			}
			if (Context.Event.Requests.bDefeatScripted)
			{
				Context.GameState->ProgramResult = BattleModeResult::ScriptedDefeat;
				Context.End = BattleEnd::Won;
				return EndKinds::ScriptedDefeat;
			}
			return Context.EventOptions.bUnscriptedDefeatIsLost ? EndKinds::Lost : EndKinds::Unsorted;
		}

		/** The event's run after the battle: the story's end in variable 0, the party's knock-outs, the result put aside. */
		void BeginRunAfterBattle(FBattleContext& Context)
		{
			FBattleEndingWork& Work = Context.Ending;
			if (!Context.bEventBattle)
			{
				return;
			}
			Context.EventVariables[0] = AfterBattleStory;
			const FBattleEventRequests& Requests = Context.Event.Requests;
			if (!Requests.bRunsAfterBattle && Requests.bEndedBattle)
			{
				return;
			}
			Context.Event.bDone = false;
			Work.SavedEnd = Context.End;
			Context.End = BattleEnd::Running;
			for (std::size_t Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
			{
				const FBattleCombatant& Member = Context.Combatants[Slot];
				Context.EventVariables[KnockOutVariables + Slot] = static_cast<uint16>((Member.Status & KnockedOut) | (Member.GearPart.Status & KnockedOut));
			}
			BeginBattleEventRun(Context);
			Work.Pc = RunAfterBattle;
		}
	}

	void BeginBattleEnding(FBattleContext& Context)
	{
		FBattleEndingWork& Work = Context.Ending;
		Work = FBattleEndingWork{};
		Work.bRunning = true;
		Work.Pc = LoadsFinishing;
		Work.Kind = SortEnd(Context);
		switch (Work.Kind)
		{
		case EndKinds::Escaped:
			Context.GameState->ProgramResult = BattleModeResult::Escaped;
			Work.Pose = EBattleEndPose::Escape;
			break;
		case EndKinds::Won:
			Context.GameState->ProgramResult = BattleModeResult::Won;
			Work.Pose = EBattleEndPose::Victory;
			BeginRunAfterBattle(Context);
			break;
		case EndKinds::Lost:
			Context.GameState->ProgramResult = BattleModeResult::Lost;
			break;
		case EndKinds::ScriptedDefeat:
			BeginRunAfterBattle(Context);
			break;
		default:
			break;
		}
	}

	bool StepBattleEnding(FBattleContext& Context)
	{
		FBattleEndingWork& Work = Context.Ending;
		while (Work.bRunning && Context.Fault == EBattleFault::None)
		{
			switch (Work.Pc)
			{
			case RunAfterBattle:
				if (StepBattleEvent(Context))
				{
					return true;
				}
				Context.End = Work.SavedEnd;
				Work.Pc = LoadsFinishing;
				break;
			case LoadsFinishing:
				Cue(Context, EBattleEndCue::FinishLoads, 0);
				if (Context.Event.Requests.bNoResults)
				{
					Work.Pose = EBattleEndPose::None;
				}
				Work.Pc = PoseTaking;
				if (ShowBlocks(Context))
				{
					return true;
				}
				break;
			case PoseTaking:
				if (ShowBlocks(Context))
				{
					return true;
				}
				Cue(Context, EBattleEndCue::Pose, static_cast<uint8>(Work.Pose));
				Work.Pc = EventEnding;
				if (ShowBlocks(Context))
				{
					return true;
				}
				break;
			case EventEnding:
				if (ShowBlocks(Context))
				{
					return true;
				}
				if (!Context.bEventBattle && (Context.End & EscapedBit) == 0 && (Context.Formation.Flags & FormationFlags::NoResultScreen) == 0)
				{
					Cue(Context, EBattleEndCue::Dim, 0);
				}
				if (Context.bEventBattle)
				{
					Work.bEventMusic = Context.Event.bMusicLoaded;
					BeginBattleEventShutdown(Context);
				}
				Work.Pc = EndingDone;
				break;
			case EndingDone:
				if (Context.bEventBattle && StepBattleEvent(Context))
				{
					return true;
				}
				if (!Work.bEventMusic && Context.End == BattleEnd::Lost)
				{
					Cue(Context, EBattleEndCue::FadeMusic, 0);
				}
				Work.bRunning = false;
				return false;
			default:
				Work.bRunning = false;
				return false;
			}
		}
		Work.bRunning = false;
		return false;
	}

	FBattleOutcome TakeBattleOutcome(const FBattleContext& Context)
	{
		const FBattleEventRequests& Requests = Context.Event.Requests;
		FBattleOutcome Outcome;
		Outcome.Result = Context.End;
		Outcome.Chain = Requests.Chain;
		Outcome.Movie = Requests.Movie;
		Outcome.bFieldStartsFresh = Requests.bFieldStartsFresh;
		Outcome.FieldReturn = Requests.FieldReturn;
		Outcome.bFanfarePlayed = Context.ResultRun.bFanfarePlayed;
		return Outcome;
	}
}
