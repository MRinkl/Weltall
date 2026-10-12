#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	struct FBattleContext;

	/**
	 * The party's turns: the command menu's two rings of commands, the attack modes (AP, the three
	 * buttons, combos and Deathblows; a gear's fuel and follow-ups), the Combo command, the Ether, Item and
	 * Special lists, the target selector, Guard, Charge, the Booster, Escape and boarding, and the
	 * automatic turns of the confused. A turn runs frame by frame as the game's turn code does: each frame
	 * takes the input's event and runs the turn until its next frame, and where the code waits for its
	 * show (the turn-start icon's flight, the queued actions' playback) the show reports back.
	 */

	/** A party member's turn at the command menu begins: its state set, the turn-start icon sent flying; runs until its first frame. */
	WELTALLCORE_API void BeginBattlePartyTurn(FBattleContext& Context, uint8 Slot);

	/** A confused party member's turn begins; it chooses and acts by itself. */
	WELTALLCORE_API void BeginBattleAutoTurn(FBattleContext& Context, uint8 Slot);

	/**
	 * One frame of a party turn: the input's step (the Ether list's scroll) and the frame's event, then the
	 * turn until its next frame. Returns false once the turn is over.
	 */
	WELTALLCORE_API bool StepBattlePartyTurn(FBattleContext& Context, uint8 Event);

	/** The menu input's step of a frame: the Ether list's scroll. */
	WELTALLCORE_API void ScrollBattleList(FBattleContext& Context);

	/** A party turn resumed with the frame's event, until its next frame; false once it is over. */
	WELTALLCORE_API bool ResumeBattlePartyTurn(FBattleContext& Context, uint8 Event);

	/**
	 * What the input makes of a frame's press for the menus (BattleMenuEvents; Nothing for none): the event,
	 * or Over when the battle has ended or the actions have played; a direction is noted as the last one
	 * pressed, and the menu's sound for it plays.
	 */
	WELTALLCORE_API uint8 TakeBattleMenuPress(FBattleContext& Context, uint8 Press);

	/**
	 * The show has played the turn's queued actions: the turn's waits end, a party member's
	 * command window closes and Billy's ammunition window with it. The show that takes the frames the window's close takes
	 * calls its halves instead.
	 */
	WELTALLCORE_API void FinishBattleActionPlayback(FBattleContext& Context);
	/**
	 * Its first half: the turn's waits end and the command window closes; whether Billy's ammunition window is up, which the
	 * HUD closes in two frames (the window's, then its work's) before CloseBattleAmmunitionWindow.
	 */
	WELTALLCORE_API bool EndBattleActionPlayback(FBattleContext& Context);
	WELTALLCORE_API void CloseBattleAmmunitionWindow(FBattleContext& Context);

	/**
	 * A slot's default target among those it can reach on the other side: the one with the lowest HP of
	 * those in its own group, else of all; the candidates are left in the context's target list.
	 */
	WELTALLCORE_API uint8 PickBattleDefaultTarget(FBattleContext& Context, uint8 Slot);

	/**
	 * A slot's default target chosen again and the way it faces at home noted (FBattleSlot::FacesLeft), as the
	 * setup and every turn's end do: left when the target's x is below the slot's own, unsigned.
	 */
	WELTALLCORE_API void AimBattleSlot(FBattleContext& Context, uint8 Slot);
}
