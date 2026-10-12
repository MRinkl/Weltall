#pragma once

#include "Battle/BattleContext.h"
#include "Game/GameState.h"
#include "Support/OracleDigest.h"
#include "Support/XorShift32.h"

#include <vector>

namespace Weltall::Testing
{
	/** A stage's layout as battle_ai_scenario.draw_stage draws it: random places and ground, routes of up to seven waypoints ended by FFh. */
	void DrawBattleStage(FXorShift32& G, FBattleStage& Stage);

	/**
	 * The AI's part of a random battle state, drawn after DrawBattleState exactly as
	 * the oracle's generator draws it: every enemy's four random scripts (into Scene,
	 * which the context's scene file then points at), its variables, the queue, the names, the enemies aimed
	 * at, the message boxes, the route, the battle event's variables, the current attack, the slots'
	 * targets, the scripts' globals and the stage. Every enemy gear's target-all ether mark is cleared first.
	 */
	void DrawBattleAiState(FXorShift32& G, FBattleContext& Context, FGameState& State, std::vector<uint8>& Scene);

	/** The AI's part in the order the oracle takes the original's memory. */
	void DigestBattleAiState(FOracleDigest& Digest, const FBattleContext& Context, const FGameState& State);

	/** A show that keeps what the rules hand it, encoded as the oracle's stubs record the original's calls. */
	class FRecordingBattleShow final : public IBattleShow
	{
	public:
		void FrameCamera(uint16 Slots) override;
		void SetMessageBox(uint8 Box, uint8 Entry) override;
		void ShowNameBox(uint8 Entry) override;
		void BeginPlayback(bool bStays, uint8 Actor, uint8 Target, uint8 NextReady) override;
		void CancelPlayback() override;
		void SetTargetMarkers(uint16 Slots) override;
		void PlaySound(uint8 Effect) override;
		void BoardGear(uint8 Slot) override;
		void BeginEvent() override {}
		void LoadBattleBank() override {}
		/** Not recorded: the rules' oracles stub the drains' show. */
		void ShowTurnDamage(uint8, const std::array<uint32, 3>&) override {}
		void PlayEventCue(const FBattleEventCue& Cue) override;
		bool IsBlocking() const override { return BlockLeft > 0; }
		void PlayTurnCue(EBattleTurnCue Cue, uint8 Arg) override;
		/** The HUD's drawing of the menus: the oracles leave the drawing out. */
		void PlayMenuCue(EBattleMenuCue, uint8, uint8) override {}
		void PlayEndCue(EBattleEndCue Cue, uint8 Arg) override;
		/** The battle event's hook, which the turn runs itself. */
		void RecordEventHook();
		/** The result screen begins, showing the gold gained (the result's own step, which the harness runs). */
		void RecordResultScreen(uint32 Gold);

		/** A frame of what the show is blocked on passes. */
		void PassFrame();

		/** The event's cues since the last take, which the harness answers as the show would. */
		std::vector<FBattleEventCue> TakeEventCues();

		/** The frames the show blocks for after a call the game waits on (a boarding, an event's special attack, voice, loads or a sequence's end); none by default. */
		uint16 BlockFrames = 0;
		/** The frames it blocks for over the battle end's loads and pose. */
		uint16 EndBlockFrames = 0;

		/** Digests the calls since the last digest (their count, then each), and forgets them. */
		void Digest(FOracleDigest& Digest);

	private:
		std::vector<uint8> Calls;
		uint16 Count = 0;
		uint16 BlockLeft = 0;
		std::vector<FBattleEventCue> Cues;
	};
}
