#pragma once

#include "Menu/MenuModuleParts.h"

#include <array>
#include <vector>

namespace Weltall
{
	/**
	 * The status sheet the Status and Gear screens draw, each group of its pieces on the layer: the
	 * portrait, the name, the labels, the numbers (each group of digits apart, as they are linked).
	 */
	struct FMenuStatusSheet
	{
		std::vector<FMenuPrimitive> Portrait;
		FMenuPrimitive Name;
		std::vector<FMenuPrimitive> Labels;
		std::vector<FMenuPrimitive> RideBadge;
		std::vector<FMenuPrimitive> Level;
		std::vector<FMenuPrimitive> Exp;
		std::vector<FMenuPrimitive> Next;
		std::vector<FMenuPrimitive> Hp;
		std::vector<FMenuPrimitive> MaxHp;
		std::vector<FMenuPrimitive> Ep;
		std::vector<FMenuPrimitive> MaxEp;
		std::vector<FMenuPrimitive> Weight;
	};

	/** The seven stats as the sheet shows them: their labels, and each one's bar and value. */
	struct FMenuStatBars
	{
		std::vector<FMenuPrimitive> Labels;
		std::array<FMenuPrimitive, 7> Bars{};
		std::array<std::vector<FMenuPrimitive>, 7> Values{};
		std::array<bool, 7> Shown{};
		/**
		 * While the Equip screen tries a candidate: each stat's change, a bar (red for a gain, blue for a
		 * loss) and a signed number; shown once any stat has changed, until the list closes.
		 */
		std::array<FMenuPrimitive, 7> ChangeBars{};
		std::array<std::vector<FMenuPrimitive>, 7> Changes{};
		bool bChangesShown = false;
	};

	/** The Skills screen's rows: each Deathblow's name and cost (or percentage), combo, learning bar. */
	struct FMenuDeathblows
	{
		std::array<FMenuLabel, 13> Names{};
		std::array<FMenuLabel, 13> Costs{};
		std::array<bool, 13> Shown{};
		std::array<std::vector<FMenuPrimitive>, 13> Combos{};
		std::array<FMenuPrimitive, 13> Bars{};
		std::array<bool, 13> BarShown{};
		/** The member's name on the layer. */
		FMenuPrimitive MemberName;
	};

	/** A party member's panel sliding between two places a few steps a frame, in 8.8 along its longer axis. */
	struct FMenuSlide
	{
		int32 FromX = 0;
		int32 ToX = 0;
		int32 FromY = 0;
		int32 ToY = 0;
		int32 StepX = 0;
		int32 StepY = 0;
		int32 MovedX = 0;
		int32 MovedY = 0;
		bool bLeftward = false;
		bool bUpward = false;
		int32 Steps = 0;
		bool bDone = false;
	};

	/** A party member's panel as the menu prepares it: the large portrait, the name, the labels and numbers. */
	struct FMenuPartyPanel
	{
		std::vector<FMenuPrimitive> Portrait;
		FMenuPrimitive Name;
		std::vector<FMenuPrimitive> Labels;
		std::vector<FMenuPrimitive> Level;
		std::vector<FMenuPrimitive> EtherLevel;
		std::vector<FMenuPrimitive> Hp;
		std::vector<FMenuPrimitive> MaxHp;
		std::vector<FMenuPrimitive> Ep;
		std::vector<FMenuPrimitive> MaxEp;
		std::vector<FMenuPrimitive> Next;
		std::vector<FMenuPrimitive> EtherNext;
	};

	/**
	 * The Equip screen: the equipment kept aside while candidates are tried on, the candidates and
	 * their labels, the description of the one highlighted or worn.
	 */
	struct FMenuEquipScreen
	{
		/** The stats as they stood when the list opened, which the tried candidates are compared with. */
		std::array<uint16, 9> SavedStats{};
		/** The slots kept aside: the weapon's five bytes, the guns' five, the accessories' five (a gear's four of each). */
		std::array<uint8, 15> SavedSlots{};
		/** The candidates by list row and how many of each the inventory holds (an accessory list starts empty: removing). */
		std::array<uint8, 200> Candidates{};
		std::array<uint8, 200> CandidateCounts{};
		std::array<FMenuLabel, 8> Names{};
		std::array<FMenuLabel, 8> Counts{};
		std::array<bool, 8> RowShown{};
		FMenuPrimitive MemberName;
		std::array<FMenuLabel, 3> Description{};
		bool bDescriptionShown = false;
	};

	/** The Items screen: a page of sixteen rows (two columns of eight), the highlighted item's line under it. */
	struct FMenuItemsScreen
	{
		std::array<FMenuLabel, 16> Names{};
		std::array<FMenuLabel, 16> Counts{};
		std::array<bool, 16> RowShown{};
		/** The highlighted item: its name and count again, its description. */
		FMenuLabel HelpName;
		FMenuLabel HelpCount;
		FMenuLabel Description;
		bool bHelpShown = false;
		/** The page's last top row, and the scroll bar's knob: its length and its step per row, in hundredths. */
		int32 LastTop = 0;
		int32 KnobHeight = 0;
		uint16 KnobStep = 0;
	};

	/**
	 * The options screen (the port's): its page's title between the L1 and R1 hints, seven rows of a label and a
	 * value under it, and the highlighted row's three lines of help in the window below.
	 */
	struct FMenuOptionsScreen
	{
		static constexpr int32 Rows = 7;
		FMenuLabel Title;
		std::array<FMenuLabel, Rows> Labels{};
		std::array<FMenuLabel, Rows> Values{};
		std::array<bool, Rows> RowShown{};
		std::array<FMenuLabel, 3> Help{};
		std::vector<FMenuPrimitive> Hints;
		/** The pictures of the rows' keys, and the keys they show (the frame's FMenuFrame::Keys). */
		std::vector<FMenuPrimitive> Cells;
		std::vector<FButtonGlyph> Keys;
		/** The page is drawn (the screen is open). */
		bool bShown = false;
	};

	/** A party member's panel in a target window: the labels, the portrait, the name, level, HP and EP. */
	struct FMenuTargetPanel
	{
		std::vector<FMenuPrimitive> Labels;
		FMenuPrimitive Portrait;
		FMenuPrimitive Name;
		std::vector<FMenuPrimitive> Level;
		std::vector<FMenuPrimitive> Hp;
		std::vector<FMenuPrimitive> MaxHp;
		std::vector<FMenuPrimitive> Ep;
		std::vector<FMenuPrimitive> MaxEp;
		bool bShown = false;
	};

	/**
	 * The Abilities screen: twelve entries in two columns (a name and a cost each; a gear's own abilities
	 * in the left column only), the EP (or fuel) and its maximum, the highlighted entry's two lines.
	 */
	struct FMenuAbilitiesScreen
	{
		std::array<FMenuLabel, 14> Names{};
		std::array<FMenuLabel, 14> Costs{};
		/** Each entry: 0 not listed, else listed (1), usable here (80h). */
		std::array<uint8, 14> Status{};
		FMenuLabel HelpName;
		std::array<FMenuLabel, 2> HelpLines{};
		bool bHelpShown = false;
		FMenuPrimitive MemberName;
	};
}
