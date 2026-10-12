#pragma once

#include "Battle/Show/BattleGameHud.h"
#include "Support/OracleDigest.h"

#include <vector>

namespace Weltall::Testing
{
	/**
	 * The game's HUD as the Battle program's oracle digests the original's:
	 * the kernel's window colour and blend as a new game has them, and each draw recorded as the SHA-1 of its packets
	 * in the GPU's order.
	 */
	class FBattleProgramHud final : public IBattleShowHud
	{
	public:
		FBattleProgramHud(const FBattleHudResources& Resources, FMesContext& Text, const FBattleEventFiles* EventFiles = nullptr);

		void Begin(FBattleContext& Context, FVram& Vram) override { Hud.Begin(Context, Vram); }
		void BeginMessageBoxes() override { Hud.BeginMessageBoxes(); }
		void Flip() override { Hud.Flip(); }
		void Draw(FBattleHudFrame& Frame, bool bHidden) override;
		void Tick(uint8 InputMode) override { Hud.Tick(InputMode); }
		void TypeMessage(uint8 Box, uint8 Entry) override { Hud.TypeMessage(Box, Entry); }
		void TypeActionName(uint8 Message) override { Hud.TypeActionName(Message); }
		void ShowNameBox(uint8 Entry) override { Hud.ShowNameBox(Entry); }
		void ShowMessageBox(int32 Box, bool bShown) override { Hud.ShowMessageBox(Box, bShown); }
		void HideMessageBoxes() override { Hud.HideMessageBoxes(); }
		void PlayMenuCue(EBattleMenuCue Cue, uint8 Slot, uint8 Arg) override { Hud.PlayMenuCue(Cue, Slot, Arg); }
		void PlayEventCue(const FBattleEventCue& Cue) override { Hud.PlayEventCue(Cue); }
		bool HasStopped() const override { return Hud.HasStopped(); }

		EBattleHudFault Fault() const { return Hud.Fault(); }
		bool IsListCursorShown() const { return Hud.IsListCursorShown(); }

		/** The draws since the last digest (their count, then each), forgotten. */
		void Digest(FOracleDigest& Digest);

	private:
		FBattleGameHud Hud;
		std::vector<uint8> Draws;
		uint16 Count = 0;
	};
}
