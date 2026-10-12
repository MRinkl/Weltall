#include "Support/BattleStepShow.h"

#include "Battle/BattleEvent.h"
#include "Battle/BattleParty.h"

namespace Weltall::Testing
{
	namespace
	{
		constexpr uint8 TurnIconFlying = 3;
		constexpr uint8 TurnIconArrived = 1;
		constexpr uint8 TurnEnd = 0xFE;
		constexpr uint8 OpeningCall = 14;
		constexpr uint8 PauseCall = 15;
		constexpr uint8 TurnDamageCall = 16;
		/** The vertical blanks EXT6's screen wave takes inside its frame, and the catch-up steps of the frame after. */
		constexpr int32 OverrunBlanks = 334;
		constexpr int32 OverrunCatchUpSteps = 4;

		void Fire(std::map<uint8, int32>& Due, int32 Frame, auto&& Report)
		{
			for (auto It = Due.begin(); It != Due.end();)
			{
				if (It->second <= Frame)
				{
					Report(It->first);
					It = Due.erase(It);
				}
				else
				{
					++It;
				}
			}
		}
	}

	void FBattleStepShow::Record(std::initializer_list<uint8> Bytes)
	{
		Calls.insert(Calls.end(), Bytes);
		++Count;
	}

	void FBattleStepShow::FrameCamera(uint16 Slots)
	{
		Record({1, static_cast<uint8>(Slots), static_cast<uint8>(Slots >> 8)});
	}

	void FBattleStepShow::SetMessageBox(uint8 Box, uint8 Entry)
	{
		Record({2, Box, Entry});
	}

	void FBattleStepShow::ShowNameBox(uint8 Entry)
	{
		Record({5, Entry});
	}

	void FBattleStepShow::BeginPlayback(bool bStays, uint8 Actor, uint8 Target, uint8 NextReady)
	{
		Record({3, static_cast<uint8>(bStays ? 1 : 0), Actor, Target, NextReady});
		BlockLeft = Times.BlockFrames;
	}

	void FBattleStepShow::CancelPlayback()
	{
		Record({6});
	}

	void FBattleStepShow::SetTargetMarkers(uint16 Slots)
	{
		Record({7, static_cast<uint8>(Slots), static_cast<uint8>(Slots >> 8)});
	}

	void FBattleStepShow::PlaySound(uint8 Effect)
	{
		Record({8, Effect});
	}

	void FBattleStepShow::BoardGear(uint8 Slot)
	{
		Record({9, Slot});
		BlockLeft = Times.BlockFrames;
	}

	void FBattleStepShow::ShowTurnDamage(uint8 Slot, const std::array<uint32, 3>& Amounts)
	{
		Calls.insert(Calls.end(), {TurnDamageCall, Slot});
		for (const uint32 Amount : Amounts)
		{
			Calls.insert(Calls.end(), {static_cast<uint8>(Amount), static_cast<uint8>(Amount >> 8), static_cast<uint8>(Amount >> 16), static_cast<uint8>(Amount >> 24)});
		}
		++Count;
		BlockLeft = Times.BlockFrames;
	}

	void FBattleStepShow::PlayEventCue(const FBattleEventCue& Cue)
	{
		Calls.insert(Calls.end(), {10, static_cast<uint8>(Cue.Cue)});
		for (const uint16 Arg : Cue.Args)
		{
			Calls.insert(Calls.end(), {static_cast<uint8>(Arg), static_cast<uint8>(Arg >> 8)});
		}
		++Count;
		const uint8 Slot = static_cast<uint8>(Cue.Args[0]);
		switch (Cue.Cue)
		{
		case EBattleEventCue::FinishPerform:
		case EBattleEventCue::SpecialAttack:
		case EBattleEventCue::FinishLoads:
		case EBattleEventCue::PlayVoice:
			BlockLeft = Times.BlockFrames;
			break;
		case EBattleEventCue::PerformSignalled:
			PerformDue[Slot] = Frames + Times.PerformFrames;
			break;
		case EBattleEventCue::CancelPerformSignals:
			PerformDue.clear();
			break;
		case EBattleEventCue::SetAnim:
		case EBattleEventCue::Attack:
		case EBattleEventCue::Walk:
		case EBattleEventCue::Jump:
			MotionDue[Slot] = Frames + Times.MotionFrames;
			break;
		default:
			break;
		}
	}

	bool FBattleStepShow::IsBlocking() const
	{
		if (BlockLeft > 0)
		{
			return true;
		}
		if (bPoseEnding)
		{
			bPoseEnding = false;
			bStarted = false;
		}
		return false;
	}

	void FBattleStepShow::PlayTurnCue(EBattleTurnCue Cue, uint8 Arg)
	{
		Record({12, static_cast<uint8>(Cue), Arg});
		if (Cue == EBattleTurnCue::ReadyNext || Cue == EBattleTurnCue::RefreshPoses)
		{
			BlockLeft = Times.BlockFrames;
		}
	}

	void FBattleStepShow::PlayEndCue(EBattleEndCue Cue, uint8 Arg)
	{
		Record({13, static_cast<uint8>(Cue), Arg});
		if (Cue == EBattleEndCue::FinishLoads || Cue == EBattleEndCue::Pose)
		{
			BlockLeft = Times.EndBlockFrames;
		}
		if (Cue == EBattleEndCue::Pose)
		{
			bPoseEnding = true;
		}
	}

	void FBattleStepShow::PlayOpeningCue(EBattleOpeningCue Cue, uint8 Arg)
	{
		Record({OpeningCall, static_cast<uint8>(Cue), Arg});
		if (Cue == EBattleOpeningCue::Presentation)
		{
			IntroLeft = Times.IntroFrames;
			FrameCamera(Context->Living);
		}
	}

	void FBattleStepShow::ShowPause(bool bPaused)
	{
		Record({PauseCall, static_cast<uint8>(bPaused ? 1 : 0)});
	}

	void FBattleStepShow::BeginFrame()
	{
		++ProgramFrames;
		const bool bOverruns = Times.OverrunEvery != 0 && ProgramFrames % Times.OverrunEvery == 0;
		CatchUp = NextCatchUp;
		NextCatchUp = bOverruns ? OverrunCatchUpSteps : 0;
		HoldLeft = bOverruns ? OverrunBlanks : 0;
		if (IntroLeft && --*IntroLeft <= 0)
		{
			bStarted = true;
			IntroLeft.reset();
		}
		for (uint8& Icon : Context->Party.TurnIcons)
		{
			if (Icon == TurnIconFlying)
			{
				Icon = TurnIconArrived;
			}
		}
		if (BlockLeft > 0)
		{
			--BlockLeft;
		}
		Fire(PerformDue, Frames + 1, [this](uint8 Slot) { FinishBattleEventPerform(*Context, Slot); });
		Fire(MotionDue, Frames + 1, [this](uint8 Slot) { FinishBattleEventMotion(*Context, Slot); });
		if (Context->Party.InputMode == BattleInputModes::Messages)
		{
			TickBattleEventMessage(*Context, Text);
		}
	}

	bool FBattleStepShow::FinishFrame()
	{
		if (Context->bActionsPlayed)
		{
			return true;
		}
		const uint8 Index = Context->PerformanceIndex;
		if (!PlaybackLeft && Index < FBattleContext::PerformanceCount && Context->Performances[Index].Code == TurnEnd)
		{
			PlaybackLeft = Times.PlaybackFrames;
		}
		if (PlaybackLeft)
		{
			if (*PlaybackLeft == 0)
			{
				PlaybackLeft.reset();
				FinishBattleActionPlayback(*Context);
			}
			else
			{
				--*PlaybackLeft;
			}
		}
		return true;
	}

	void FBattleStepShow::Digest(FOracleDigest& Digest)
	{
		Digest.I16(Count);
		Digest.Raw(Calls);
		Calls.clear();
		Count = 0;
	}
}
