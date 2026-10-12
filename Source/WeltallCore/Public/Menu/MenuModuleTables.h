#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	/** An orb and the label beside it on one of the menus' arcs: two frames of the sprite sheet. */
	struct FMenuArcCommand
	{
		int32 Orb = 0;
		int32 Label = 0;
	};

	/**
	 * A party member's panel's places: its nine labels' (the sheet's frames in FMenuMemberTables), then
	 * the portrait, the level, the ether level, HP, max HP, EP, max EP and the name, each the first
	 * panel's, the others a step further down.
	 */
	struct FMenuPanelPlaces
	{
		static constexpr int32 Labels = 9;
		static constexpr std::size_t Portrait = 9;
		static constexpr std::size_t Level = 10;
		static constexpr std::size_t Hp = 12;
		static constexpr std::size_t MaxHp = 13;
		static constexpr std::size_t Ep = 14;
		static constexpr std::size_t MaxEp = 15;
		static constexpr std::size_t Name = 16;

		std::array<int32, 17> X{};
		std::array<int32, 17> Y{};
	};

	/** Member Change's module's tables (directory 16, file 6): its panels' and pointers' places and its names' strips. */
	struct FMenuMemberTables
	{
		/** The pointer's places: the party's three rows, then the reserve's six. */
		std::array<int32, 9> PointerX{};
		std::array<int32, 9> PointerY{};
		FMenuPanelPlaces Party;
		FMenuPanelPlaces Reserve;
		/** The panels' labels: frames of the sprite sheet, 0xFFFF for none. */
		std::array<int32, 9> PanelLabels{};
		/** Where each pair of names (0 and 1, 2 and 3...) is typed: its unit in the page at x 384, its row. */
		std::array<int32, 6> NameStripX{};
		std::array<int32, 6> NameStripY{};
		/** The text labels the module renders as it starts (never shown). */
		std::array<uint8, 4> Labels{};

		/** From the module as the game loads it (at 801C5000). */
		WELTALLCORE_API static TResult<FMenuMemberTables> Parse(std::span<const uint8> Module);
	};

	/** Enter Name's module's tables (directory 16, file 8). */
	struct FMenuNameTables
	{
		/**
		 * The character grid: glyph indices in four blocks of nine rows of six (five characters and a
		 * spacer), 0x0F and 0xCF blank, 0x1F "End", 0xFF the rows' end; then the module's bytes after
		 * it to its end, which the cursor can reach past the grid.
		 */
		std::vector<uint8> Grid;
		/** The text labels the module renders as it starts: the buttons' "Accept" and "Cancel" among them. */
		std::array<uint8, 4> Labels{};

		WELTALLCORE_API static TResult<FMenuNameTables> Parse(std::span<const uint8> Module);
	};

	/** A few pictures of the sheet at their places: frames, and their x and y. */
	template <std::size_t Count>
	struct FMenuPlacedFrames
	{
		std::array<uint8, Count> Frames{};
		std::array<int32, Count> X{};
		std::array<int32, Count> Y{};
	};

	/** The Shop's module's tables (directory 16, file 9). */
	struct FMenuShopTables
	{
		/** The arcs' places: the shop's three (Exit, Sell, Buy), then Sell's four. */
		std::array<int32, 7> ArcX{};
		std::array<int32, 7> ArcY{};
		/** The shop's arc, then the inner arcs by the shop's command. */
		std::array<FMenuArcCommand, 3> MainCommands{};
		std::array<std::array<FMenuArcCommand, 4>, 3> SubCommands{};
		/** The text labels' messages: beside the shop's arc, beside Sell's, a list's heading ("Stored"). */
		std::array<uint8, 4> MainLabels{};
		std::array<uint8, 4> SubLabels{};
		std::array<uint8, 2> ListLabels{};
		std::array<int32, 4> MainLabelOffsets{};
		std::array<int32, 4> SubLabelOffsets{};
		/** The dialogs' pointers (the last two: Yes and No). */
		std::array<int32, 4> ChoiceX{};
		std::array<int32, 4> ChoiceY{};
		std::array<uint8, 4> HelpLabels{};
		/**
		 * The available characters' faces, one place each from the left: nine, then the words after the
		 * table, which a tenth and eleventh available character would take.
		 */
		std::array<int32, 11> FaceX{};
		/** The lists' fixed pictures: Buy's four, Sell's two. */
		FMenuPlacedFrames<4> BuyStatics;
		FMenuPlacedFrames<2> SellStatics;
		/** The gold on entry, the list's total and the gold left. */
		std::array<int32, 3> GoldX{};
		std::array<int32, 3> GoldY{};

		WELTALLCORE_API static TResult<FMenuShopTables> Parse(std::span<const uint8> Module);
	};

	/** The gear shop's module's tables (directory 16, file 10). */
	struct FMenuGearShopTables
	{
		static constexpr int32 Gears = 17;
		/** An inner arc's row ends at this frame. */
		static constexpr int32 NoCommand = 0xFFFF;
		/** The scanner's pieces have no place for this choice. */
		static constexpr uint8 NoScan = 0xFF;

		/** The arcs' places: the shop's four (Exit, Sell, Buy, Tune up), then an inner arc's. */
		std::array<int32, 9> ArcX{};
		std::array<int32, 9> ArcY{};
		std::array<FMenuArcCommand, 4> MainCommands{};
		/** The inner arcs by the shop's command (Exit's is never opened), each row ended by NoCommand. */
		std::array<std::array<FMenuArcCommand, 4>, 4> SubCommands{};
		/** The text labels' messages: beside the shop's arc, beside Sell's and Buy's, beside Tune up's, the lists' headings ("Stored", "Equip"). */
		std::array<uint8, 4> MainLabels{};
		std::array<uint8, 4> SubLabels{};
		std::array<uint8, 4> TuneLabels{};
		std::array<uint8, 2> ListLabels{};
		std::array<int32, 4> MainLabelOffsets{};
		std::array<int32, 4> SubLabelOffsets{};
		std::array<int32, 4> TuneLabelOffsets{};
		/** The dialogs' pointers (the last two: Yes and No). */
		std::array<int32, 4> ChoiceX{};
		std::array<int32, 4> ChoiceY{};
		std::array<uint8, 4> HelpLabels{};
		/** The pilots' faces, one place each from the left: nine, then the words after the table. */
		std::array<int32, 11> FaceX{};
		/** The lists' fixed pictures: Buy's four, Tune up's four, Sell's two. */
		FMenuPlacedFrames<4> BuyStatics;
		FMenuPlacedFrames<4> TuneStatics;
		FMenuPlacedFrames<2> SellStatics;
		/** The gold on entry, the list's total and the gold left. */
		std::array<int32, 3> GoldX{};
		std::array<int32, 3> GoldY{};
		/**
		 * The camera on a choice: its distance by gear; its height by gear and choice, at
		 * ((command + 3 x gear) x 4 + inner command); the gear's turn by choice, at command x 4 + inner command.
		 */
		std::array<int16, Gears> ZoomDistance{};
		std::array<int16, (3 * Gears + 1) * 4> ZoomHeight{};
		std::array<int16, 16> ZoomTurn{};
		/** The L1 and R1 hints' x. */
		std::array<int32, 2> HintX{};
		/**
		 * The scanner: each bracket's frames (five steps of four parts), where each piece stops by choice
		 * (command x 4 + inner command: a place of six, or NoScan), the brackets' x and their heights by
		 * place, the panel's places and the flicker's.
		 */
		std::array<std::array<uint16, 20>, 2> BracketFrames{};
		std::array<uint8, 16> ScanPlaces{};
		std::array<int16, 2> BracketX{};
		std::array<std::array<int16, 6>, 2> BracketY{};
		std::array<int16, 6> PanelX{};
		std::array<int16, 6> PanelY{};
		std::array<int16, 6> FlickerX{};
		std::array<int16, 6> FlickerY{};
		/** The status panel: its fourteen labels and their places, where its numbers start (HP, max HP, fuel, max fuel, weight). */
		std::array<uint16, 14> StatusFrames{};
		std::array<int16, 14> StatusX{};
		std::array<int16, 14> StatusY{};
		std::array<std::array<int16, 2>, 5> StatusNumbers{};
		/** Each gear's pilot, as the module's own table has it: seventeen, then the bytes after it, which the gears past it read. */
		std::array<uint8, 20> Pilots{};

		WELTALLCORE_API static TResult<FMenuGearShopTables> Parse(std::span<const uint8> Module);
	};
}
