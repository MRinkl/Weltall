#pragma once

#include "Gears/GearShopModel.h"
#include "Menu/MenuShopModule.h"

#include <array>
#include <optional>
#include <vector>

namespace Weltall
{
	/** What the gear shop's lists hold, in their order: Tune up's three, then Buy's and Sell's two. */
	enum class EGearGoods : uint8
	{
		Armor = 0,
		Frame = 1,
		Engine = 2,
		Part = 3,
		Weapon = 4,
	};

	/**
	 * The shop's camera on its gear: the distance and height it stands at and the gear's turn, each
	 * moving to a target a step a frame (16.16), the distance and height together along the longer
	 * of the two, the turn spread over as many frames.
	 */
	struct FGearShopCamera
	{
		/** Which of the distance (1), the height (2) and the turn (4) still move. */
		uint8 Moving = 0;
		int32 Distance = 0x400;
		int32 Height = 0;
		std::array<int32, 3> From{0x400, 0, -0x400};
		std::array<int32, 3> To{0x400, 0, -0x400};
		std::array<int32, 3> Steps{};
		std::array<int32, 3> Moved{};
		std::array<bool, 3> bDown{};
		/** The distance and height move this many steps a frame. */
		int32 Pace = 0x10;
	};

	/** The scanner's pieces over a zoomed gear: two brackets, a panel and a flickering mark, each opening, running, closing. */
	struct FGearShopScanner
	{
		bool bShown = false;
		bool bFlickerShown = false;
		std::array<uint8, 2> BracketStates{};
		uint8 PanelState = 0;
		std::array<uint8, 2> BracketDelays{};
		uint8 PanelDelay = 0;
		uint8 FlickerDelay = 0;
		uint8 FlickerCel = 0;
		std::array<int16, 2> BracketSteps{};
		int16 PanelStep = 0;
		std::array<uint16, 2> BracketX{};
		std::array<uint16, 2> BracketY{};
		uint16 PanelX = 0;
		uint16 PanelY = 0;
		uint16 FlickerX = 0;
		uint16 FlickerY = 0;
		std::array<std::vector<FMenuPrimitive>, 2> Brackets{};
		std::vector<FMenuPrimitive> Panel;
		std::array<std::vector<FMenuPrimitive>, 3> Flicker{};
	};

	/**
	 * The gear shop (mode 5) as its module runs: the pilots' gears shown one at a time as a lit 3D model
	 * under the shop's camera, their parts and gear weapons bought and sold, their armor, frame and engine
	 * tuned up, their fuel restored; the camera zooms on the part chosen while a scanner plays over it.
	 */
	class FMenuGearShop final : public FMenuShopModule
	{
	public:
		FMenuGearShop(const FMenuServices& InServices, const FMenuRequest& InRequest);

		TMenuTask<> Run() override;
		const FGearShopModel* GearModel() const override { return &Model; }

	private:
		void DrawFrame() override;
		void AddLabels();
		void AddStatusPanel();
		/** The shop's own table of the pilots' gears, the lists, the model: false when they cannot be made. */
		bool Init();
		void LoadLists();
		TMenuTask<> Shutdown();
		TMenuTask<> RunTopMenu();
		/** The command chosen run: false for Exit. */
		TMenuTask<bool> EnterSubmenu();
		TMenuTask<> RunSubmenu();
		/** The choice made in a sub-arc: the camera on the part, the list, the camera back. */
		TMenuTask<> RunChoice();

		// The model, the camera and the scanner (MenuGearShopView.cpp).
		/** The shown pilot's gear's files read, the model made from them and the status panel drawn. */
		std::optional<FError> ReadGearFiles(uint8 Gear);
		std::optional<FError> ShowGear(uint8 Gear);
		/** The model's texture blocks and VRAM copies since the last taken, into the menu's VRAM. */
		void TakeModelVram();
		/** The next pilot's (or, bPrevious, the one before's) gear shown, while the camera rests. */
		TMenuTask<> SwitchGear(bool bPrevious);
		void BuildStatusPanel();
		void PlanCameraMove();
		void StepCamera();
		TMenuTask<> ZoomToPart();
		void ZoomOut();
		void AnimateScanner();
		void AnimateBrackets();
		void AnimatePanel();
		void AnimateFlicker();
		/** The scanner's place for the choice made (by the command and the inner command). */
		uint8 ScanPlace() const;

		// Buying and tuning up (MenuGearShopBuy.cpp).
		/** A list bought from: Buy's parts or gear weapons, or (bTuning) Tune up's armors, frames or engines. */
		TMenuTask<> RunBuyList(bool bTuning);
		uint32 RenderBuyRows(int32 Top, uint32 Budget, std::array<uint8, 8>& Usable);
		uint32 ShowBuyDetail(int32 Index);
		void ShowOwnedCount(EGearGoods Kind, uint8 Id);
		void ShowEquippedName(EGearGoods Kind);
		/** The change the goods would make to a pilot's gear's attack and defense: their sizes, and whether each is a loss. */
		void PreviewStats(uint8 Id, EGearGoods Kind, uint8 Pilot, std::array<int32, 2>& Changes, std::array<bool, 2>& Losses);
		void CommitPurchase(uint32 Left);
		/** The fitted armor, frame or engine is lower than or equal to the goods at Index (by id). */
		bool IsUpgrade(int32 Index) const;
		/** The gold, the list's total, the gold left (and Tune up's trade-in), moved Offset places for Tune up. */
		void DrawMoneyPanel(uint32 OnEntry, uint32 Total, uint32 Left, uint32 TradeIn, int32 Offset);

		// Selling and restoring (MenuGearShopSell.cpp).
		TMenuTask<> RunSell();
		TMenuTask<> RunSellList(std::span<uint8> Ids, std::span<uint8> Counts, EGearGoods Kind);
		void RenderSellRows(int32 Top);
		uint32 ShowSellDetail(uint8 Id, EGearGoods Kind);
		void CommitSale(uint32 Gold, std::span<uint8> Ids, std::span<uint8> Counts);
		/** The list's scene cleared; bCloses also closes its windows, cursor and scroll bar. */
		void ClearListScreen(bool bCloses);
		TMenuTask<> RunRestore();
		void DrawGoldPanel(uint32 Gold);
		/**
		 * A dialog: FirstText's lines asked Yes or No (bWaits: until Confirm or Cancel; else a notice any
		 * button or 60 frames end), or with a SecondText, FirstText's notice first, then SecondText asked.
		 */
		TMenuTask<bool> Ask(uint8 FirstText, std::optional<uint8> SecondText, bool bWaits);

		// The goods (MenuGearShopGoods.cpp).
		std::span<const uint8> GoodsRecord(uint8 Id, EGearGoods Kind) const;
		std::span<const uint8> GoodsName(uint8 Id, EGearGoods Kind) const;
		uint32 GoodsPrice(uint8 Id, EGearGoods Kind) const;
		/** The gears that may be fitted with the goods: a bit by gear. */
		uint32 GoodsUsers(uint8 Id, EGearGoods Kind) const;
		/** The available pilots' gears the goods are fitted to: a bit by gear. */
		uint32 FittedMask(uint8 Id, EGearGoods Kind) const;
		/** The gear's armor, frame, engine, parts and weapons applied to it, as the shop's own copy does. */
		void ApplyGear(uint8 Gear);
		/** The gear's attack and defense as the shop compares them. */
		std::array<int16, 2> GearStats(uint8 Gear);
		uint8 GearOf(int32 Character) const;

		const FMenuGearShopTables& Tables;
		/** The shop's lists by EGearGoods and how many each holds. */
		std::array<std::array<uint8, 20>, 5> Lists{};
		std::array<int32, 5> ListCounts{};
		/** The gears' pilots, as the shop's own table has them (the grown Emeralda takes her gear once the story says so). */
		std::array<uint8, 20> Pilots{};
		uint8 CurrentGear = 0;
		int32 PilotCount = 0;
		int32 PilotIndex = 0;
		std::optional<FGearFiles> PendingFiles;

		FGearShopModel Model;
		bool bModelShown = false;
		FGearShopCamera Camera;
		FGearShopScanner Scanner;

		// The status panel: the gear's name, its labels and numbers, the shop's title.
		bool bStatusShown = false;
		FMenuPrimitive NameQuad;
		std::vector<FMenuPrimitive> StatusLabels;
		std::array<std::vector<FMenuPrimitive>, 5> StatusNumbers{};
		std::vector<FMenuPrimitive> Title;
		std::vector<FMenuPrimitive> MemberHints;
		bool bMemberHintsShown = false;

		// A list's entries: the goods, their kinds, what the cart holds of each; Buy's and Tune up's.
		std::array<uint8, 48> Entries{};
		std::array<EGearGoods, 48> EntryKinds{};
		std::array<uint8, 48> Cart{};
		/** The inventory's count of the highlighted goods. */
		uint8 OwnedCount = 0;
		/** The pilots' gears' attack and defense when the list opened, by character. */
		std::array<std::array<int16, 16>, 2> ListedStats{};

		// A sell list, compacted: the goods, how many are owned and to be sold.
		std::vector<uint8> SaleIds;
		std::vector<uint8> SaleOwned;
		std::vector<uint8> SaleSold;
		std::vector<bool> SaleListed;
		int32 SaleCount = 0;
		EGearGoods SaleKind = EGearGoods::Part;
	};
}
