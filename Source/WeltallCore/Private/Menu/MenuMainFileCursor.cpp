#include "Game/SaveStore.h"
#include "Menu/MenuMain.h"

namespace Weltall
{
	namespace
	{
		constexpr uint8 Free = FMenuFileScreen::Free;
		constexpr int32 CellsPerCard = FMenuFileScreen::CellsPerCard;
		/** The cursor's places: a card's 15 cells, then the other's. */
		constexpr int32 Places = 2 * CellsPerCard;
		/** Right and Left move a row's width of places: three cells down a card's columns. */
		constexpr int32 RowStep = 3;
		constexpr int32 AnyFile = 0;
		constexpr int32 AnySave = 1;
		constexpr int32 AnyCell = 2;
		constexpr int32 Chosen = 1;
		constexpr int32 Cancelled = 2;
		/** The cards drawn with the cursor's save picked out. */
		constexpr uint8 CursorDrawn = 2;
	}

	int32 FMenuMain::CursorCell() const
	{
		return Files.Cursor >= 0 && Files.Cursor < Places ? Resources.Main().CursorCells[static_cast<std::size_t>(Files.Cursor)] : FMenuFileScreen::NoCell;
	}

	bool FMenuMain::CardShown(int32 Card) const
	{
		// The original tests the byte after the two cards' for a third card, its lists' state: no place lies there.
		return Card >= 0 && Card < FMenuFileScreen::Cards;
	}

	bool FMenuMain::CellSelectable(int32 Mode, int32 Place) const
	{
		const std::size_t Cell = static_cast<std::size_t>(Resources.Main().CursorCells[static_cast<std::size_t>(Place)]);
		return Files.Cells[Cell] != Free && (Mode == AnyFile || Files.CellIsSave[Cell]);
	}

	void FMenuMain::ResetCellCursor()
	{
		Files.DetailsCursor = Free;
		Files.Cursor = 0;
		// On the save last made, where its store is shown: a store's saves fill its cells by their slots.
		const std::optional<int32> Last = Services.Saves != nullptr ? Services.Saves->LastSaved() : std::nullopt;
		if (!Last)
		{
			return;
		}
		const int32 Card = *Last / FSaveStore::SlotsPerStore - Files.FirstStore;
		if (Card < 0 || Card >= FMenuFileScreen::Cards)
		{
			return;
		}
		int32 Place = Card * CellsPerCard;
		for (int32 Slot = 0; Slot < *Last % FSaveStore::SlotsPerStore; ++Slot)
		{
			TResult<std::optional<std::vector<uint8>>> Bytes = Services.Saves->ReadBytes(StoreSlot(Card, Slot));
			Place += Bytes && *Bytes ? 1 : 0;
		}
		Files.Cursor = Place;
	}

	bool FMenuMain::IsCellSelectable(int32 Mode)
	{
		const bool bAnyFiles = Files.Listed[0] != 0 || Files.Listed[1] != 0;
		const int32 Cell = CursorCell();
		bool bSelectable = false;
		if (Cell == FMenuFileScreen::NoCell)
		{
			return false;
		}
		if (Mode == AnyCell)
		{
			bSelectable = CardShown(Cell / 16);
		}
		else if (Mode == AnyFile || Mode == AnySave)
		{
			bSelectable = bAnyFiles && CardShown(Cell / 16) && CellSelectable(Mode, Files.Cursor);
		}
		if (bSelectable)
		{
			Files.Drawn = CursorDrawn;
		}
		return bSelectable;
	}

	int32 FMenuMain::FirstSelectableCell(int32 Mode)
	{
		const bool bAnyFiles = Files.Listed[0] != 0 || Files.Listed[1] != 0;
		int32 Found = Free;
		for (int32 Place = 0; Place < Places && (Mode == AnyCell || bAnyFiles); ++Place)
		{
			const int32 Card = Resources.Main().CursorCells[static_cast<std::size_t>(Place)] / 16;
			if (CardShown(Card) && (Mode == AnyCell || CellSelectable(Mode, Place)))
			{
				Found = Place;
				break;
			}
		}
		Files.Drawn = CursorDrawn;
		return Found;
	}

	void FMenuMain::CursorRight(int32 Mode, int32 From)
	{
		if (Mode == AnyCell)
		{
			const int32 To = From + RowStep;
			if (CardShown(To / CellsPerCard) && To < Places)
			{
				Files.Cursor = To;
			}
			return;
		}
		// The same row on, a card's width at a time and into the other card; else the next place from four on.
		for (int32 To = From + RowStep; To < Places; To += RowStep)
		{
			if (CellSelectable(Mode, To))
			{
				Files.Cursor = To;
				return;
			}
		}
		for (int32 To = Files.Cursor + RowStep + 1; To < Places; ++To)
		{
			if (CellSelectable(Mode, To))
			{
				Files.Cursor = To;
				return;
			}
		}
	}

	void FMenuMain::CursorLeft(int32 Mode, int32 From)
	{
		if (Mode == AnyCell)
		{
			const int32 To = From - RowStep;
			if (CardShown(To / CellsPerCard) && To >= 0)
			{
				Files.Cursor = To;
			}
			return;
		}
		for (int32 To = From - RowStep; To >= 0; To -= RowStep)
		{
			if (CellSelectable(Mode, To))
			{
				Files.Cursor = To;
				return;
			}
		}
		for (int32 To = Files.Cursor - RowStep - 1; To >= 0; --To)
		{
			if (CellSelectable(Mode, To))
			{
				Files.Cursor = To;
				return;
			}
		}
	}

	void FMenuMain::CursorDown(int32 Mode, int32 From)
	{
		if (Mode == AnyCell)
		{
			const int32 To = From + 1;
			if (CardShown(To / CellsPerCard) && To < Places)
			{
				Files.Cursor = To;
			}
			return;
		}
		for (int32 To = From + 1; To < Places; ++To)
		{
			if (CellSelectable(Mode, To))
			{
				Files.Cursor = To;
				return;
			}
		}
	}

	void FMenuMain::CursorUp(int32 Mode, int32 From)
	{
		if (Mode == AnyCell)
		{
			const int32 To = From - 1;
			if (CardShown(To / CellsPerCard) && To >= 0)
			{
				Files.Cursor = To;
			}
			return;
		}
		for (int32 To = From - 1; To >= 0; --To)
		{
			if (CellSelectable(Mode, To))
			{
				Files.Cursor = To;
				return;
			}
		}
	}

	int32 FMenuMain::UpdateCellCursor(int32 Mode)
	{
		int32 Result = 0;
		switch (Code)
		{
		case EMenuCode::Right:
			CursorRight(Mode, Files.Cursor);
			break;
		case EMenuCode::Down:
			CursorDown(Mode, Files.Cursor);
			break;
		case EMenuCode::Left:
			CursorLeft(Mode, Files.Cursor);
			break;
		case EMenuCode::Up:
			CursorUp(Mode, Files.Cursor);
			break;
		case EMenuCode::Confirm:
			Result = Chosen;
			break;
		case EMenuCode::Cancel:
			Result = Cancelled;
			break;
		case EMenuCode::L1:
		case EMenuCode::R1:
			// The other pair's details follow once its stores are read, at the next frame.
			TurnStorePair(Code == EMenuCode::L1 ? -1 : 1);
			return 0;
		default:
			break;
		}
		if (Files.Cursor != Files.DetailsCursor && CursorCell() != FMenuFileScreen::NoCell)
		{
			ShowFileDetails(Files.Cells[static_cast<std::size_t>(CursorCell())]);
			Files.DetailsCursor = Files.Cursor;
		}
		return Result;
	}

	int32 FMenuMain::PickSaveSlot(int32 Card, int32 Replaced) const
	{
		if (Replaced != Free)
		{
			return Replaced;
		}
		for (int32 Slot = 0; Slot < CellsPerCard; ++Slot)
		{
			if (!Files.SlotUsed[static_cast<std::size_t>(Card * FMenuFileScreen::PerCard + Slot)])
			{
				return Slot;
			}
		}
		return 0;
	}
}
