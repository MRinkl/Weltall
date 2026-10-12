#pragma once

#include "Menu/MenuModule.h"

#include <array>
#include <vector>

namespace Weltall
{
	/** A member's panel in Member Change: its labels, portrait, name, level and HP and EP with their maxima. */
	struct FMenuMemberPanel
	{
		std::vector<FMenuPrimitive> Labels;
		std::vector<FMenuPrimitive> Portrait;
		FMenuPrimitive Name;
		std::vector<FMenuPrimitive> Level;
		std::vector<FMenuPrimitive> Hp;
		std::vector<FMenuPrimitive> MaxHp;
		std::vector<FMenuPrimitive> Ep;
		std::vector<FMenuPrimitive> MaxEp;
		bool bShown = false;
	};

	/**
	 * Member Change (mode 1) as its module runs: the party's column and the reserve's, a member picked in
	 * one and swapped with one of the other, the party written back compacted.
	 */
	class FMenuMember final : public FMenuModule
	{
	public:
		FMenuMember(const FMenuServices& InServices, const FMenuRequest& InRequest);

		TMenuTask<> Run() override;

	private:
		void DrawFrame() override;
		void Init();
		void BuildReserve();
		void RenderNamePair(int32 Pair);
		TMenuTask<> ChooseParty();
		/** The picked member and the one under the cursor swapped, when both are there and neither is kept in the party. */
		bool Swap(int32 Column, int32 Row, int32 Top, int32 PickedColumn, int32 PickedRow, int32 PickedTop);
		TMenuTask<> Shutdown();

		// The panels and the pointers (MenuMemberPanels.cpp).
		TMenuTask<> BuildPanels(int32 Top);
		void BuildPanel(FMenuMemberPanel& Panel, uint8 Character, int32 Row, const FMenuPanelPlaces& Places, int32 Step);
		void DrawDigits(std::vector<FMenuPrimitive>& Digits, uint32 Value, int32 First, int32 Count, int32 X, int32 Y, bool bPacked);
		void PlacePointer(int32 Column, int32 Row, int32 Which);
		void AddPointers();
		void AddPanel(const FMenuMemberPanel& Panel);

		/** Which characters can be chosen: joined and allowed. */
		std::array<bool, 16> Available{};
		/** The game's party slot each row of the party's column shows: the column holds the party in its player's order. */
		std::array<int8, 3> GameSlots{0, 1, 2};
		/** Whether the column shows the order its player arranged this party in, which a newcomer then takes a place in. */
		bool bArranged = false;
		/** The reserve: the available characters outside the party, in id order, 0xFF past them. */
		std::array<uint8, 11> Reserve{};
		std::array<FMenuMemberPanel, 3> PartyPanels{};
		std::array<FMenuMemberPanel, 6> ReservePanels{};
		bool bPanelsShown = false;
		/** The cursor's pointer and the pick's. */
		std::array<std::vector<FMenuPrimitive>, 2> Pointers{};
		std::array<bool, 2> PointerShown{};
		bool bPointersShown = false;
		std::array<FMenuLabel, 4> Labels{};
	};
}
