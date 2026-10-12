#pragma once

#include "Battle/BattleContext.h"
#include "Battle/BattleEvent.h"
#include "Game/GameState.h"
#include "Support/OracleDigest.h"
#include "Support/XorShift32.h"
#include "Text/MesWindow.h"

#include <array>
#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace Weltall::Testing
{
	/** A run of the event after its load: the story's variable as the enemies left it, and for the run after the battle the party's KO bits. */
	struct FBattleEventRunCase
	{
		std::optional<uint16> Story;
		std::optional<std::array<uint16, 3>> KnockedOut;
	};

	/** The event of a random battle: its script and messages, how the show answers, the presses, the runs. */
	struct FBattleEventCase
	{
		std::vector<uint8> Script;
		std::vector<uint8> Messages;
		/** Frames the show takes to end a combatant's motion and a signalled sequence, and to finish what the event waits on. */
		uint8 MotionFrames = 0;
		uint8 PerformFrames = 0;
		uint8 BlockFrames = 0;
		/** The confirm button is pressed in the frames whose number plus the phase the period divides. */
		uint8 PressPeriod = 1;
		uint8 PressPhase = 0;
		/** The opening's run, then the turn boundaries'. */
		std::vector<FBattleEventRunCase> Runs;
		bool bShutdown = false;
	};

	/**
	 * The event's part of a random battle, drawn after DrawBattleState exactly as
	 * the oracle's generator draws it: the game state's names and party, the
	 * stage, a party gear place freed, the disc's script (or a random one with the disc's messages), the
	 * show's frames, the presses and the runs.
	 */
	FBattleEventCase DrawBattleEventState(FXorShift32& G, FBattleContext& Context, FGameState& State, const FBattleEventArchive& Archive);

	/** The event's state in the order the oracle takes the original's memory. */
	/**
	 * The event's state as the oracles digest it. The original keeps the "more" arrow's shown flag in the HUD, where the
	 * lists' cursor sets it too: a test with the HUD passes the HUD's flag, the others take the event's own.
	 */
	void DigestBattleEventState(FOracleDigest& Digest, const FBattleContext& Context, std::optional<bool> ListCursorShown = std::nullopt);

	class FRecordingBattleShow;

	/** A case's frames as the event oracle's stubs play them: the show's reports, the HUD's message, the message input, the digest. */
	class FBattleEventFrames
	{
	public:
		/** A case stops after this many frames. */
		static constexpr int32 FrameCap = 1500;

		FBattleEventFrames(FBattleContext& InContext, FRecordingBattleShow& InShow, FMesContext& InText, const FBattleEventCase& InCase, FOracleDigest& InDigest)
			: Context(InContext), Show(InShow), Text(InText), Case(InCase), Digest(InDigest)
		{
		}

		/**
		 * A routine stepped to its end with the frames between (Step true while it waits for a frame); false
		 * when the case's frames ran out first, or where the game would hang (the case ends there).
		 */
		template <typename FStep>
		bool Run(FStep&& Step)
		{
			while (Step())
			{
				TakeCues();
				if (!Frame())
				{
					return false;
				}
			}
			return Finish();
		}

		/** The event's routine to its end. */
		bool Run();

		/** The event's state and the show's calls since the last digest, then what the test adds (Extra). */
		void DigestEvent();

		/** What a test digests after the event's state each time. */
		std::function<void(FOracleDigest&)> Extra;

	private:
		void TakeCues();
		bool Frame();
		bool Finish();

		FBattleContext& Context;
		FRecordingBattleShow& Show;
		FMesContext& Text;
		const FBattleEventCase& Case;
		FOracleDigest& Digest;
		int32 Frames = 0;
		std::map<uint8, int32> PerformDue;
		std::map<uint8, int32> MotionDue;
	};
}
