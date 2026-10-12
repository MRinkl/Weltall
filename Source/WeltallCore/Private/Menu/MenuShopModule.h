#pragma once

#include "Menu/MenuModule.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	/** A row of a shop's list: its name and its number (a price or a count), and the cart's mark and quantity. */
	struct FMenuShopRow
	{
		FMenuLabel Name;
		FMenuLabel Number;
		std::vector<FMenuPrimitive> Cart;
		bool bShown = false;
	};

	/**
	 * What a shop's list shows beside its rows: the available characters' faces (outlined in red for
	 * those who can use the goods, marked "E" for those who wear them), the lines, the gold, the
	 * highlighted goods' line, the changes a purchase would make, the dialog's total; in the gear shop
	 * also the fitted part's name, the part traded in and the gold beside the fuel's dialog.
	 */
	struct FMenuShopScene
	{
		bool bShown = false;
		std::vector<FMenuPrimitive> Faces;
		std::array<bool, 9> Outlined{};
		std::array<FMenuPrimitive, 9> OutlineTops{};
		std::array<FMenuPrimitive, 9> OutlineBottoms{};
		std::vector<FMenuPrimitive> Statics;
		std::vector<FMenuPrimitive> Worn;
		std::array<FMenuShopRow, 8> Rows{};
		FMenuLabel Equipped;
		bool bEquippedShown = false;
		FMenuLabel Detail;
		bool bDetailShown = false;
		FMenuLabel SalePrice;
		bool bSalePriceShown = false;
		FMenuLabel Stored;
		bool bStoredShown = false;
		bool bGoldShown = false;
		FMenuPrimitive GoldLine;
		std::array<std::vector<FMenuPrimitive>, 3> Gold{};
		std::vector<FMenuPrimitive> TradeIn;
		bool bTradeInShown = false;
		std::array<std::vector<FMenuPrimitive>, 9> AttackChanges{};
		std::array<std::vector<FMenuPrimitive>, 9> DefenseChanges{};
		std::vector<FMenuPrimitive> Total;
		std::vector<FMenuPrimitive> GoldPanel;
		bool bGoldPanelShown = false;
	};

	/**
	 * What the shop and the gear shop share, each module with its own copy in the original: the lists'
	 * scene (the faces, the rows, the gold, the cursor and the scroll bar), the dialogs and their Yes
	 * and No pointers, the text labels they place.
	 */
	class FMenuShopModule : public FMenuModule
	{
	protected:
		FMenuShopModule(const FMenuServices& InServices, const FMenuRequest& InRequest, uint16 InDialogWidth);

		// The dialogs (MenuShopDialog.cpp).
		TMenuTask<> OpenDialog(uint8 FirstText);
		TMenuTask<> CloseDialog();
		/**
		 * The answer to the dialog shown: with bChoice, Left and Right move between Yes and No and the
		 * answer is where Confirm leaves it (Cancel: No); without, only Confirm or Cancel end it (No). A
		 * notice (!bWaits) also ends at a button already pressed or after 60 frames.
		 */
		TMenuTask<bool> RunYesNo(bool bWaits, bool bChoice);
		/** Yes (true) or No, the cursor starting on No. */
		TMenuTask<bool> AskYesNo(uint8 FirstText);
		void OpenChoiceCursors();
		TMenuTask<> CloseChoiceCursors();
		void AddChoiceCursors();

		// The lists' scene (MenuShopScene.cpp).
		/** The available characters' faces, one more character a frame, from FaceX's places on. */
		TMenuTask<> RevealFaces(std::span<const int32> FaceX);
		void DrawStatics(std::span<const uint8> Frames, std::span<const int32> X, std::span<const int32> Y);
		/** A number's digits First to First + Count as the sheet's frames, from X on (packed: blank places left out). */
		void DrawDigitsInto(std::vector<FMenuPrimitive>& Digits, uint32 Value, int32 First, int32 Count, int32 X, int32 Y, bool bPacked);
		/** The dialog's total, in nine places from X + 3. */
		void DrawTotal(uint32 Total, int32 X, int32 Y);
		void DrawListCursor(int32 Row);
		void DrawScrollThumb(int32 Entries, int32 Top);
		/** A row's name and number typed and placed: the number right-aligned in five glyphs. */
		void DrawRow(int32 Row, std::span<const uint8> Name, uint32 Number, uint8 Mode, uint8 NumberMode);
		void DrawCartMark(int32 Row, int32 Mark, uint8 Quantity);
		/** The glyphs of a number right-aligned in five places, its leading zeros blank. */
		static std::array<uint16, 5> NumberGlyphs(uint32 Number);
		void AddScene();
		/** The list's scene hidden and emptied. */
		void ClearScene();

		/** Which characters the shop shows. */
		std::array<bool, 16> Available{};
		FMenuShopScene Scene;
		/** The dialog's total shown: 2 drawn, 1 once the dialog is up. */
		uint8 TotalState = 0;
		std::array<FMenuLabel, 4> HelpLabels{};
		std::array<FMenuLabel, 3> DialogLines{};
		bool bDialogShown = false;
		/** The dialogs' pointers' places (the last two beside Yes and No). */
		std::span<const int32> ChoiceX;
		std::span<const int32> ChoiceY;
		std::array<std::vector<FMenuPrimitive>, 4> ChoiceCursors{};
		std::array<bool, 4> ChoiceShown{};
		bool bChoiceCursorsShown = false;
		FMenuLayerSprite ListCursor;
		bool bListCursorShown = false;
		FMenuLayerSprite ScrollThumb;
		bool bScrollThumbShown = false;
		std::array<FMenuLabel, 8> ArcLabels{};
		FVramImage DetailImage;
		FMesWindow DetailTyping{0x39, 1};

	private:
		uint16 DialogWidth = 0;
	};
}
