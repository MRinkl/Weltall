#pragma once

#include "Battle/BattlePartyState.h"
#include "Battle/Show/BattleShowCockpit.h"
#include "Foundation/Types.h"
#include "Graphics/Vram.h"
#include "Menu/MenuPrimitive.h"

#include <array>
#include <span>

namespace Weltall
{
	/** A primitive the HUD keeps once for each of the two draw buffers, as the game builds its HUD's. */
	using FBattleHudPair = std::array<FMenuPrimitive, 2>;

	/**
	 * The command ring's pictures: two layers of 50 parts in one run, as the game keeps them. A layer drawn past its 50
	 * runs into the next one's first parts (a gear's outer ring takes 51), which those states leave unlinked.
	 */
	struct FBattleHudCommandRing
	{
		static constexpr int32 LayerParts = 50;
		static constexpr int32 Layers = 2;

		std::array<FBattleHudPair, LayerParts * Layers> Parts{};
		std::array<int32, Layers> Counts{};
		/** The buffer the ring was built in, which it is linked and faded from. */
		uint8 Buffer = 0;
	};

	/** The target cursor's four arrows (right, down, left, up): a triangle per way and buffer, its tip pulsing red. */
	struct FBattleHudArrows
	{
		static constexpr int32 Ways = 4;

		std::array<FBattleHudPair, Ways> Triangles{};
		/** The tip's red, pulsing by 4 between 40h and FFh, and which way it goes. */
		int32 Glow = 0;
		bool bFalling = false;
		/** The buffer the arrows were made in, the only one drawn. */
		uint8 Buffer = 0;
		/** The ways a target lies in, whose arrows are drawn. */
		std::array<bool, Ways> Targets{};
		bool bShown = false;
	};

	/** A run of sheet sprites the lists draw in the show work (their icons, scroll bar and cursor), its count and buffer. */
	template <int32 Size>
	struct FBattleHudSpriteRun
	{
		std::array<FBattleHudPair, Size> Parts{};
		int32 Count = 0;
		uint8 Buffer = 0;
		bool bShown = false;
	};

	/** One of the battle's labels typed as a list's work is made: the plane and texels of its text row, and its width. */
	struct FBattleHudLabel
	{
		uint8 Width = 0;
		uint8 Plane = 0;
		uint8 U = 0;
		uint8 V = 0;
	};

	/**
	 * The lists' text block (show work +A230h), made as a list opens and freed as it closes: its text quads, each list
	 * using them its own way (by the block's offset / 50h: the cells 0-3, the info 4, the figures 5-7, the help 8-9, the
	 * labels 10-11, the Ether list's 12-18), and what shows them.
	 */
	struct FBattleHudListWork
	{
		static constexpr int32 QuadCount = 19;
		static constexpr int32 Cells = 0;
		static constexpr int32 Info = 4;
		static constexpr int32 Figures = 5;
		static constexpr int32 Help = 8;
		static constexpr int32 Labels = 10;
		/** The Ether list's: the rate's label, then the EP's label, digits and slash. */
		static constexpr int32 Rate = 12;
		static constexpr int32 Points = 13;

		std::array<FBattleHudPair, QuadCount> Quads{};
		/** The Combo list's stored AP: a red bar as long as the AP left, and the black rest of its track. */
		FBattleHudPair ComboBar{};
		FBattleHudPair ComboRest{};
		bool bComboShown = false;
		uint8 CellsBuffer = 0;
		bool bCellsShown = false;
		uint8 InfoBuffer = 0;
		bool bInfoShown = false;
		uint8 FiguresBuffer = 0;
		bool bCountShown = false;
		uint8 FigureCount = 0;
	};

	/**
	 * A list's cursor and what its loop last drew (the game keeps them in the list routine's own locals): the cursor's
	 * frame counting down 4-0 every third draw, the cell, row and scroll the info and the rows were drawn for, and how
	 * many Deathblows the Combo list's queue was drawn with.
	 */
	struct FBattleHudListLoop
	{
		int32 CursorFrame = 4;
		uint8 CursorTicks = 0;
		uint8 DrawnCell = 0xFF;
		uint8 DrawnRow = 0xFF;
		int32 DrawnScroll = 0xFFFF;
		uint8 DrawnQueued = 0xFF;
	};

	/**
	 * The attack mode's hints (the command HUD's block while a party member's turn runs): the buttons pressed, and the
	 * Deathblows the next presses would make, each its buttons and its name; in a gear, each with its fuel cost.
	 */
	struct FBattleHudHints
	{
		static constexpr int32 NameParts = 3;
		static constexpr int32 SpriteParts = 20;

		std::array<FBattleHudPair, NameParts> Names{};
		std::array<FBattleHudPair, SpriteParts> Sprites{};
		std::array<FBattleHudPair, NameParts> Costs{};
		int32 NameCount = 0;
		int32 SpriteCount = 0;
		int32 CostCount = 0;
		uint8 NameBuffer = 0;
		uint8 SpriteBuffer = 0;
		uint8 CostBuffer = 0;
	};

	/** A party member's panel on the result screen: its runs of sheet sprites, each with the room the game's block gives it. */
	struct FBattleHudResultPanel
	{
		FBattleHudSpriteRun<4> Portrait;
		FBattleHudSpriteRun<20> Labels;
		FBattleHudSpriteRun<3> Level;
		FBattleHudSpriteRun<3> EtherLevel;
		FBattleHudSpriteRun<3> Hp;
		FBattleHudSpriteRun<3> MaxHp;
		FBattleHudSpriteRun<2> Ep;
		FBattleHudSpriteRun<2> MaxEp;
		/** The EXP as it counts up and what is left to add, and the ether's in a row of their own. */
		FBattleHudSpriteRun<8> Exp;
		FBattleHudSpriteRun<8> EtherExp;
		FBattleHudSpriteRun<7> ExpGain;
		FBattleHudSpriteRun<7> EtherExpGain;
	};

	/** A stat's row of the level-up window: its value before the level-up, the change's sign and digits, and its two bars. */
	struct FBattleHudResultStat
	{
		FBattleHudSpriteRun<3> Value;
		FBattleHudSpriteRun<4> Change;
		/** The bar as long as the value was, and the change's after it (a rise) or over its end (a fall). */
		FBattleHudPair Bar{};
		FBattleHudPair ChangeBar{};
		uint8 BarBuffer = 0;
	};

	/**
	 * The result screen's work, made as the screen begins: the three panels, and what its windows show: a member's
	 * level-up, a learned skill's message, the items and gold.
	 */
	struct FBattleHudResult
	{
		static constexpr int32 StatCount = 7;
		static constexpr int32 ItemRows = 8;

		std::array<FBattleHudResultPanel, 3> Panels{};
		/** The level-up window: the member's portrait (the learned skills' too), the labels, max HP and EP as they were and their changes, the stats. */
		FBattleHudSpriteRun<4> Portrait;
		FBattleHudSpriteRun<67> Labels;
		FBattleHudSpriteRun<3> OldMaxHp;
		FBattleHudSpriteRun<2> OldMaxEp;
		FBattleHudSpriteRun<4> MaxHpChange;
		FBattleHudSpriteRun<3> MaxEpChange;
		std::array<FBattleHudResultStat, StatCount> Stats{};
		/** A learned skill's message: its name's quad and the "learned." label after it, each drawn from the buffer it was placed in. */
		FBattleHudPair LearnedLabel{};
		FBattleHudPair LearnedName{};
		uint8 LearnedLabelBuffer = 0;
		uint8 LearnedNameBuffer = 0;
		/** The image a member's learned skills' names are typed into, one over the other. */
		FVramImage LearnedImage;
		/** The gold: its labels, the gold gained and the party's gold. */
		FBattleHudSpriteRun<7> GoldLabels;
		FBattleHudSpriteRun<6> GoldGained;
		FBattleHudSpriteRun<9> GoldTotal;
		/** The two headings, and each listed drop's name and count. */
		std::array<FBattleHudPair, 2> Headings{};
		uint8 HeadingsBuffer = 0;
		std::array<FBattleHudPair, ItemRows> ItemNames{};
		std::array<FBattleHudPair, ItemRows> ItemCounts{};
		uint8 ItemCount = 0;
		uint8 ItemsBuffer = 0;
	};

	/**
	 * The gear cockpit (the command HUD's block while a gear's turn runs): its sprites in runs of parts, each with the
	 * room the game's block gives it, its count and its buffer.
	 */
	struct FBattleHudCockpit
	{
		enum ERun : uint8
		{
			/** The cockpit's frame, the bearing's scale along the top, the reticle and its sight. */
			Frame,
			Bearing,
			Reticle,
			Sight,
			/** The two blended layers of the cockpit's glass: added, then subtracted. */
			Glass,
			Shade,
			/** The elevation's scale beside the reticle. */
			Elevation,
			Booster,
			Unused,
			/** The attack level's name, and its shade. */
			Level,
			LevelShade,
			/** The twenty digits that flicker while the reticle moves. */
			Digits,
			/** The warning lamps, which blink. */
			Lamps,
			Attack,
			InfinityChance,
			ArmorBreak,
			Agility,
			Fuel,
			MaxFuel,
			RunCount,
		};

		/** Where each run begins in the block, and the block's parts. */
		static constexpr std::array<int32, RunCount> First{0, 12, 42, 50, 74, 119, 162, 188, 199, 200, 217, 226, 246, 251, 255, 258, 262, 264, 268};
		static constexpr int32 Parts = 273;

		struct FRun
		{
			int32 Count = 0;
			uint8 Buffer = 0;
		};

		std::array<FBattleHudPair, Parts> Sprites{};
		std::array<FRun, RunCount> Runs{};

		/** A run's parts: as many as lie before the next run's. */
		std::span<FBattleHudPair> Room(ERun Run)
		{
			const int32 End = Run + 1 < RunCount ? First[Run + 1] : Parts;
			return std::span<FBattleHudPair>(Sprites).subspan(static_cast<std::size_t>(First[Run]), static_cast<std::size_t>(End - First[Run]));
		}
		/** The lamps' blink: drawn for 14 frames of every 21. */
		int16 LampTicks = 0;
	};
}
