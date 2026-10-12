#pragma once

#include "Battle/BattleProgramShow.h"
#include "Support/OracleDigest.h"
#include "Text/MesWindow.h"

#include <map>
#include <optional>
#include <vector>

namespace Weltall::Testing
{
	/** How long the step oracle's harness show takes over each thing it plays. */
	struct FBattleStepShowTimes
	{
		/** Frames from the presentation's start to the intro's end. */
		uint8 IntroFrames = 1;
		/** Frames the actions take to play once their end is queued. */
		uint8 PlaybackFrames = 0;
		/**
		 * Frames what the rules and the event wait on take (the playback's start, a boarding, the turn's drains, the
		 * next actor readied, the poses, a special attack, a voice, loads, a sequence's end).
		 */
		uint8 BlockFrames = 0;
		/** Frames the end's loads and pose take. */
		uint8 EndBlockFrames = 0;
		/** Frames to an event's motion's end and to a signalled sequence's end. */
		uint8 MotionFrames = 1;
		uint8 PerformFrames = 1;
		/**
		 * Every so many of the program's frames one overruns as EXT6's screen wave makes a frame: it takes the wave's
		 * vertical blanks before its input step, and the frame after it runs the most catch-up steps. 0: none does.
		 */
		int32 OverrunEvery = 0;
	};

	/**
	 * The show as the step oracle's harness plays it for the Battle
	 * program: every call recorded in order as the rules' oracles record them; the intro ending a drawn number
	 * of frames after the presentation began; a turn-start icon arriving at once; the actions finishing a
	 * drawn number of frames after their end is queued; an event's motions and signalled sequences ending,
	 * and what the rules, the event and the end wait on taking, the drawn frames; the event's message typed
	 * a tick a frame; the end pose ending the battle's start as the battle takes it up again.
	 */
	class FBattleStepShow final : public IBattleProgramShow
	{
	public:
		FBattleStepShow(FMesContext& InText, const FBattleStepShowTimes& InTimes) : Text(InText), Times(InTimes) {}

		void BeginBattle(FBattleContext& InContext) override { Context = &InContext; }
		void FrameCamera(uint16 Slots) override;
		void SetMessageBox(uint8 Box, uint8 Entry) override;
		void ShowNameBox(uint8 Entry) override;
		void BeginPlayback(bool bStays, uint8 Actor, uint8 Target, uint8 NextReady) override;
		void CancelPlayback() override;
		void SetTargetMarkers(uint16 Slots) override;
		void PlaySound(uint8 Effect) override;
		void BoardGear(uint8 Slot) override;
		void ShowTurnDamage(uint8 Slot, const std::array<uint32, 3>& Amounts) override;
		/** The event's load and its runs' starts: the oracle's harness answers the original's sound calls there and records none. */
		void BeginEvent() override {}
		void LoadBattleBank() override {}
		void PlayEventCue(const FBattleEventCue& Cue) override;
		bool IsBlocking() const override;
		void PlayTurnCue(EBattleTurnCue Cue, uint8 Arg) override;
		/** The HUD's drawing of the menus: the oracles leave the drawing out. */
		void PlayMenuCue(EBattleMenuCue, uint8, uint8) override {}
		void PlayEndCue(EBattleEndCue Cue, uint8 Arg) override;

		void PlayOpeningCue(EBattleOpeningCue Cue, uint8 Arg) override;
		void BeginFrame() override;
		bool IsHolding() const override { return HoldLeft > 0; }
		void HoldBlank() override { --HoldLeft; }
		int32 CatchUpSteps() const override { return CatchUp; }
		/** The oracle's harness runs no frame of the show's own: its buffer never flips from the second. */
		bool DrewFirstBuffer() const override { return false; }
		bool FinishFrame() override;
		bool HasStarted() const override { return bStarted; }
		void TickHud(uint8) override {}
		void ShowPause(bool bPaused) override;
		bool ResumeRules() override { return true; }
		bool HasStopped() const override { return false; }

		/** A frame of the program has ended (the harness counts every one, those a pause holds included). */
		void EndFrame() { ++Frames; }

		/** Digests the calls since the last digest (their count, then each), and forgets them. */
		void Digest(FOracleDigest& Digest);

	private:
		void Record(std::initializer_list<uint8> Bytes);

		FBattleContext* Context = nullptr;
		FMesContext& Text;
		FBattleStepShowTimes Times;
		std::vector<uint8> Calls;
		uint16 Count = 0;
		int32 Frames = 0;
		std::optional<int32> IntroLeft;
		std::optional<int32> PlaybackLeft;
		std::map<uint8, int32> PerformDue;
		std::map<uint8, int32> MotionDue;
		/** The program's frames begun, the blanks the frame begun still takes for itself, its catch-up steps and the next frame's. */
		int32 ProgramFrames = 0;
		int32 HoldLeft = 0;
		int32 CatchUp = 0;
		int32 NextCatchUp = 0;
		/** The frames the show still blocks for; the end pose ends the battle's start once its frames have passed. */
		int32 BlockLeft = 0;
		mutable bool bPoseEnding = false;
		mutable bool bStarted = false;
	};
}
