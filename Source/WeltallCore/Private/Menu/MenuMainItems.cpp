#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Game/ItemUse.h"
#include "Menu/MenuMain.h"
#include "Menu/MenuResources.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** The item list's inventory: its first list, 150 slots, a page of sixteen (two columns). */
		constexpr int32 ItemsList = 0;
		constexpr int32 Slots = 150;
		constexpr int32 PageSlots = 16;
		constexpr int32 LastCursor = PageSlots - 1;
		constexpr uint8 MostHeld = 99;
		/** The scroll bar's knob: its place, its length without scrolling and with, the track it moves along. */
		constexpr int32 KnobX = 0x0C;
		constexpr int32 KnobY = 0x12;
		constexpr int32 KnobFull = 0x74;
		constexpr int32 KnobScrolling = 0x4A;
		constexpr int32 KnobTrack = 0x1068;
		constexpr int32 PageRowsSkipped = 8;
		/** The rows: names and counts two columns apart, their strips two a row pair from (384, 128). */
		constexpr int32 ColumnStep = 0x88;
		constexpr int32 NameX = 0x28;
		constexpr int32 CountX = 0x90;
		constexpr int32 RowTop = 0x0E;
		constexpr int32 RowHeight = 13;
		constexpr int16 StripX = 0x180;
		constexpr int16 StripY = 0x80;
		constexpr int16 StripColumn = 0x18;
		constexpr int16 StripWidth = 0x28;
		constexpr int32 StripBase = 0x80;
		constexpr uint16 FirstDigitGlyph = 0x10;
		constexpr uint16 BlankGlyph = 0xC3;
		/** The highlighted item's line: its name and count again, its description from the strip at (320, 78). */
		constexpr int32 HelpNameX = 0x10;
		constexpr int32 HelpY = 0x93;
		constexpr int32 HelpCountX = 0x78;
		constexpr int32 DescriptionX = 0x1C;
		constexpr int32 DescriptionY = 0xA1;
		constexpr int16 DescriptionStripX = 0x140;
		constexpr int16 DescriptionStripY = 0x4E;
		constexpr int16 DescriptionStripWidth = 0x3C;
		/** The kernel's name table of the items. */
		constexpr int32 ItemNames = 22;
		constexpr int32 PiecesEntry = 4;
		constexpr uint8 Bright = 0x80;
	}

	std::span<const uint8> FMenuMain::ItemRecordOf(uint8 Item) const
	{
		const std::span<const uint8> Items = Resources.Items();
		const std::size_t At = static_cast<std::size_t>(Item) * ItemRecord::Size;
		return At + ItemRecord::Size <= Items.size() ? Items.subspan(At, ItemRecord::Size) : std::span<const uint8>{};
	}

	void FMenuMain::OpenItems()
	{
		InitTargetPointers(2);
		RenderLabels(HelpTags, std::span<const uint8>(Resources.Main().LabelSets).first(HelpTags.size()), 4);
		ItemList = {};
	}

	void FMenuMain::CloseItems()
	{
		Flags[MenuFlag::ScrollKnob] = 0;
		CloseWindow(3);
		CloseWindow(4);
		Flags[MenuFlag::ItemList] = 0;
	}

	void FMenuMain::CountItemRows()
	{
		// The last slot holding an item: a list of more than a page scrolls by rows of two.
		const std::span<const uint8> Items = Services.Game->InventoryItems(ItemsList);
		int32 Last = 0;
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			if (Items[static_cast<std::size_t>(Slot)] != 0)
			{
				Last = Slot;
			}
		}
		if (Last < PageSlots)
		{
			ItemList.KnobHeight = KnobFull;
			ItemList.LastTop = 0;
			ItemList.KnobStep = 0;
		}
		else
		{
			ItemList.LastTop = (Last - PageSlots) / 2 + 1;
			ItemList.KnobHeight = KnobScrolling;
			ItemList.KnobStep = static_cast<uint16>(KnobTrack / ItemList.LastTop);
		}
	}

	TMenuTask<bool> FMenuMain::ItemsScreen()
	{
		bool bGoesOn = true;
		bool bFirst = true;
		int32 DrawnTop = 0xFF;
		int32 Cursor = 0;
		int32 DrawnCursor = 0xFF;
		int32 Mark = 0xFF;
		int32 Top = 0;
		OpenItems();
		CountItemRows();
		AllocCursor(0);
		AllocCursor(1);
		while (true)
		{
			co_await Frame();
			if (Top != DrawnTop)
			{
				DrawItemPage(Top);
				DrawScrollKnob(KnobX, static_cast<int32>(static_cast<uint32>(ItemList.KnobStep) * static_cast<uint32>(Top)) / 100 + KnobY, ItemList.KnobHeight);
				DrawnTop = Top;
			}
			DrawCursor(Cursor, Top, 0, 0);
			if (Cursor != DrawnCursor)
			{
				DrawItemHelp(Cursor, Top);
				DrawnCursor = Cursor;
			}
			if (bFirst)
			{
				OpenWindow(3, 0x0C, 10, 0x124, 0x84, false, true, PiecesEntry, true);
				OpenWindow(4, 8, 0x8F, 0x130, 0x22, false, true, PiecesEntry, false);
				bFirst = false;
				OpenLayer();
				co_await SlideParty(false, false);
			}
			DrawCursor(Mark, Top, 1, 1);
			switch (Code)
			{
			case EMenuCode::Right:
				// Past the page's last slot the list scrolls a row and the cursor comes back to the left column.
				if (Cursor + 1 <= LastCursor)
				{
					++Cursor;
				}
				else if (Top + 1 <= ItemList.LastTop)
				{
					++Top;
					Cursor = LastCursor - 1;
				}
				DrawnCursor = 0xFF;
				break;
			case EMenuCode::Down:
				if (Cursor + 2 <= LastCursor)
				{
					Cursor += 2;
				}
				else if (Top + 1 <= ItemList.LastTop)
				{
					++Top;
				}
				DrawnCursor = 0xFF;
				break;
			case EMenuCode::Left:
				if (Cursor - 1 >= 0)
				{
					--Cursor;
				}
				else if (Top - 1 >= 0)
				{
					--Top;
					Cursor = 1;
				}
				DrawnCursor = 0xFF;
				break;
			case EMenuCode::Up:
				if (Cursor - 2 >= 0)
				{
					Cursor -= 2;
				}
				else if (Top - 1 >= 0)
				{
					--Top;
				}
				DrawnCursor = 0xFF;
				break;
			case EMenuCode::Confirm:
				// A slot is marked; confirmed again it is used, another slot is swapped with it.
				if (Mark == 0xFF)
				{
					Mark = Top * 2 + Cursor;
					break;
				}
				if (Top * 2 + Cursor == Mark)
				{
					Mark = 0xFF;
					if (!co_await UseItem(Top, Cursor))
					{
						break;
					}
				}
				else
				{
					const std::size_t From = static_cast<std::size_t>(Top * 2 + Cursor);
					const std::size_t To = static_cast<std::size_t>(Mark);
					const std::span<uint8> Items = Services.Game->InventoryItems(ItemsList);
					const std::span<uint8> Counts = Services.Game->InventoryCounts(ItemsList);
					std::swap(Items[From], Items[To]);
					std::swap(Counts[From], Counts[To]);
				}
				DrawnTop = 0xFF;
				DrawnCursor = 0xFF;
				Mark = 0xFF;
				break;
			case EMenuCode::Cancel:
				if (Mark != 0xFF)
				{
					Mark = 0xFF;
				}
				else
				{
					bGoesOn = false;
				}
				break;
			case EMenuCode::R1:
				DrawnCursor = 0xFF;
				Top = std::min(Top + PageRowsSkipped, ItemList.LastTop);
				break;
			case EMenuCode::L1:
				DrawnCursor = 0xFF;
				Top = std::max(Top - PageRowsSkipped, 0);
				break;
			default:
				break;
			}
			if (!bGoesOn)
			{
				Flags[MenuFlag::TargetPointers] = 0;
				Flags[MenuFlag::Cursors] = 0;
				Flags[MenuFlag::Cursors + 1] = 0;
				ClearFlags(MenuFlag::HelpTags, static_cast<int32>(HelpTags.size()));
				co_return true;
			}
		}
	}

	void FMenuMain::DrawItemPage(int32 Top)
	{
		// The page's sixteen slots, the inventory repaired as they are drawn: an empty slot holds nothing,
		// a count of 0 leaves the slot empty, and no count shows more than 99.
		const std::span<uint8> Items = Services.Game->InventoryItems(ItemsList);
		const std::span<uint8> Counts = Services.Game->InventoryCounts(ItemsList);
		std::fill(NameImage.Units.begin(), NameImage.Units.end(), uint16{0});
		for (int32 Row = 0; Row < PageSlots; ++Row)
		{
			const std::size_t Slot = static_cast<std::size_t>(Top * 2 + Row);
			const std::size_t At = static_cast<std::size_t>(Row);
			if (Items[Slot] == 0)
			{
				Counts[Slot] = 0;
				ItemList.RowShown[At] = false;
				continue;
			}
			if (Counts[Slot] == 0)
			{
				Items[Slot] = 0;
				ItemList.RowShown[At] = false;
				continue;
			}
			if (Counts[Slot] > MostHeld)
			{
				Counts[Slot] = MostHeld;
			}
			FMenuLabel& Name = ItemList.Names[At];
			FMenuLabel& Count = ItemList.Counts[At];
			Name.Width = static_cast<uint8>(RenderString(NameTyping, NameImage, Services.Text.Tables->String(ItemNames, Items[Slot]), 0));
			const uint8 Held = Counts[Slot];
			const std::array<uint16, 2> Glyphs = {static_cast<uint16>(Held / 10 == 0 ? BlankGlyph : Held / 10 + FirstDigitGlyph),
				static_cast<uint16>(Held % 10 + FirstDigitGlyph)};
			GlyphText(Glyphs, GlyphScratch);
			Count.Width = static_cast<uint8>(RenderString(NameTyping, NameImage, GlyphScratch, 1));
			NameImage.Rect = {static_cast<int16>(StripX + (Row & 1) * StripColumn), static_cast<int16>(StripY + Row / 2 * RowHeight), StripWidth,
				static_cast<int16>(RowHeight)};
			Upload(NameImage);
			// Items that cannot be used here are dimmed: those outside the menu, and those only for save points elsewhere.
			const std::span<const uint8> Record = ItemRecordOf(Items[Slot]);
			const uint8 Use = Record.empty() ? 0 : Record[ItemRecord::Use];
			const uint8 Usable = (Use & ItemRecord::AtSavePoints) != 0 && Request.Param == 0 ? 0 : static_cast<uint8>(Use & ItemRecord::InMenu);
			SetStripLabelTexture(Name, Row, StripBase, static_cast<uint8>(Usable | 1));
			SetStripLabelTexture(Count, Row, StripBase, static_cast<uint8>(Usable | 2));
			const int32 Column = Row % 2 * ColumnStep;
			const int32 Y = (Row / 2 & 0xFFF) << 4 | RowTop;
			SetLayerQuad(Name.Quad.Points, (Column + NameX) & 0xFFF8, Y, Name.Width, RowHeight);
			SetLayerQuad(Count.Quad.Points, (Column + CountX) & 0xFFF8, Y, Count.Width, RowHeight);
			RecordLabel(Name);
			RecordLabel(Count);
			ItemList.RowShown[At] = true;
		}
		Flags[MenuFlag::ItemList] = 1;
	}

	void FMenuMain::DrawItemHelp(int32 Cursor, int32 Top)
	{
		const uint8 Item = Services.Game->InventoryItems(ItemsList)[static_cast<std::size_t>(Top * 2 + Cursor)];
		if (Item == 0)
		{
			ClearFlags(MenuFlag::HelpTags, static_cast<int32>(HelpTags.size()));
			ItemList.bHelpShown = false;
			return;
		}
		// The description, typed alone into its strip.
		std::fill(HelpImage.Units.begin(), HelpImage.Units.end(), uint16{0});
		FMenuLabel& Description = ItemList.Description;
		Description.Width = static_cast<uint8>(RenderString(HelpTyping, HelpImage, Resources.Text(FMenuResources::EText::ItemHelp).String(Item), 0));
		HelpImage.Rect = {DescriptionStripX, DescriptionStripY, DescriptionStripWidth, static_cast<int16>(RowHeight)};
		Upload(HelpImage);
		SetLabelTexture(Description, 0, 0);
		RecordLabel(Description);
		SetTexelRect(Description.Quad, 0, DescriptionStripY, Description.Width, RowHeight);
		SetLayerQuad(Description.Quad.Points, DescriptionX, DescriptionY, Description.Width, RowHeight);

		// The name and count again, bright whatever the list shows.
		const std::size_t Row = static_cast<std::size_t>(Cursor);
		ItemList.HelpName = ItemList.Names[Row];
		ItemList.HelpCount = ItemList.Counts[Row];
		SetLayerQuad(ItemList.HelpName.Quad.Points, HelpNameX, HelpY, ItemList.Names[Row].Width, RowHeight);
		SetLayerQuad(ItemList.HelpCount.Quad.Points, ItemList.Counts[Row].Width == 0x10 ? HelpCountX | 4 : HelpCountX, HelpY, ItemList.Counts[Row].Width, RowHeight);
		RecordLabel(ItemList.HelpName);
		RecordLabel(ItemList.HelpCount);
		for (FMenuPrimitive* Quad : {&ItemList.HelpName.Quad, &ItemList.HelpCount.Quad})
		{
			Quad->Corners[0].Color = {Bright, Bright, Bright};
			Quad->Code = static_cast<uint8>(Quad->Code & ~MenuGpu::SemiTransparent);
		}

		// The tags of what an item for the menu or battle works on: the side, then the extent.
		ClearFlags(MenuFlag::HelpTags, static_cast<int32>(HelpTags.size()));
		const std::span<const uint8> Record = ItemRecordOf(Item);
		if (!Record.empty() && (Record[ItemRecord::Use] & (ItemRecord::InMenu | ItemRecord::InBattle)) != 0)
		{
			const uint16 Targeting = LittleEndian::ReadU16(Record, ItemRecord::Targeting);
			const int32 Side = (Targeting & ItemRecord::OnSelf) != 0 ? 2 : ((Targeting & ItemRecord::OnEnemy) == 0 ? 1 : 0);
			const int32 Extent = (Targeting & 3) + 3;
			for (const int32 Tag : {Side, Extent})
			{
				FMenuLabel& Placed = HelpTags[static_cast<std::size_t>(Tag)];
				SetLayerQuad(Placed.Quad.Points, Resources.Main().HelpTagX[static_cast<std::size_t>(Tag)], Resources.Main().HelpTagY, Placed.Width, RowHeight);
				RecordLabel(Placed);
				Flags[static_cast<std::size_t>(MenuFlag::HelpTags + Tag)] = 1;
			}
			for (const int32 Tag : {Side, Extent})
			{
				FMenuPrimitive& Quad = HelpTags[static_cast<std::size_t>(Tag)].Quad;
				Quad.Corners[0].Color = {Bright, Bright, Bright};
				Quad.Code = static_cast<uint8>(Quad.Code & ~MenuGpu::SemiTransparent);
			}
		}
		ItemList.bHelpShown = true;
	}

	void FMenuMain::AddItemList()
	{
		if (Flags[MenuFlag::ItemList] == 0)
		{
			return;
		}
		for (std::size_t Row = 0; Row < ItemList.RowShown.size(); ++Row)
		{
			if (ItemList.RowShown[Row])
			{
				AddProjected(ItemList.Names[Row].Quad, true, PiecesEntry);
				AddProjected(ItemList.Counts[Row].Quad, true, PiecesEntry);
			}
		}
		if (ItemList.bHelpShown)
		{
			AddProjected(ItemList.HelpName.Quad, true, PiecesEntry);
			AddProjected(ItemList.HelpCount.Quad, true, PiecesEntry);
			AddProjected(ItemList.Description.Quad, true, PiecesEntry);
		}
	}
}
