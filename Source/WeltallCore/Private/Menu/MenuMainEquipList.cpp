#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Game/GameStateLayout.h"
#include "Menu/MenuMain.h"
#include "Menu/MenuResources.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		using LittleEndian::ReadU16;
		using LittleEndian::ReadU32;

		constexpr int32 ListRows = 8;
		/** The list's rows: a name, then how many the inventory holds, 13 rows apart. */
		constexpr int32 NameX = 0xA8;
		constexpr int32 CountX = 0x10C;
		constexpr int32 FirstRowY = 0x12;
		constexpr int32 RowHeight = 13;
		/** The rows' strips: two a row pair from (384, 128). */
		constexpr int16 StripX = 0x180;
		constexpr int16 StripY = 0x80;
		constexpr int16 StripColumn = 0x18;
		constexpr int16 StripWidth = 0x28;
		constexpr int32 StripBase = 0x80;
		constexpr uint8 NamePlane = 0x81;
		constexpr uint8 CountPlane = 0x82;
		/** The glyphs a count is written with: a digit's is 10h on, a blank for a tens of 0. */
		constexpr uint16 FirstDigitGlyph = 0x10;
		constexpr uint16 BlankGlyph = 0xC3;
		/** The description: three lines under the list, the strips' rows 4 and 5. */
		constexpr int32 DescriptionX = 0x10;
		constexpr int32 DescriptionY = 0x96;
		constexpr int32 DescriptionLines = 3;
		constexpr int32 DescriptionStrip = 8;
		/** The kernel's name tables: weapons, accessories, gear weapons, gear parts. */
		constexpr int32 WeaponNames = 23;
		constexpr int32 AccessoryNames = 17;
		constexpr int32 GearWeaponNames = 51;
		constexpr int32 GearPartNames = 50;
		/** The candidates' kinds: a weapon, one of Billy's loads (by his page's slot, 1 to 4), an accessory or part. */
		constexpr int32 WeaponKind = 0;
		constexpr int32 AccessoryKind = 5;
		/** Ammunition and the like start here among the weapons; only lower ids are weapons proper. */
		constexpr uint8 FirstAmmunition = 0x32;
		constexpr uint8 WeaponTypes = 5;
		constexpr std::size_t RecordSize = 0x10;
		constexpr std::size_t GearWeaponSize = 0x14;
		constexpr int32 PiecesEntry = 4;

		int32 SlotLabelCount(int32 Page) { return Page == 0 ? 2 : 4; }
	}

	int32 FMenuMain::BuildEquipList(int32 Slot, int32 Top, int32 Which, int32 Page, bool bGear)
	{
		FGameState& Game = *Services.Game;
		const uint8 Character = Member(Slot);
		const uint8 Gear = Game.CharacterGear(Character).value_or(FGameState::EmptySlot);
		const FEquipmentTables& Tables = Resources.Equipment();
		const std::span<const uint8> GearWeapons = Resources.GearWeapons();
		const FGearPartTables& PartTables = GearParts;
		const auto GearWeapon = [&GearWeapons](uint8 Id) { return GearWeapons.subspan(std::min<std::size_t>(Id * GearWeaponSize, GearWeapons.size())); };

		// The kind of candidate, and for accessories or parts the classes the other slots wear.
		int32 Kind = WeaponKind;
		std::size_t Slots = 100;
		uint16 ClassHere = 0;
		uint16 ClassesWorn = 0;
		std::span<const uint8> Gun;
		if (Page == 0)
		{
			if (Which != 0)
			{
				Kind = AccessoryKind;
				Slots = bGear ? 150 : 200;
				const std::size_t Here = static_cast<std::size_t>(9 + Which);
				const auto Class = [&](uint8 Id) -> uint16
				{
					if (bGear)
					{
						const std::span<const uint8> Part = PartTables.Part(Id);
						return Part.size() >= 0x0A ? ReadU16(Part, 8) : 0;
					}
					const std::span<const uint8> Accessory = Tables.Accessory(Id);
					return Accessory.size() >= RecordSize ? ReadU16(Accessory, 0x0E) : 0;
				};
				ClassHere = Class(EquipList.SavedSlots[Here]);
				for (std::size_t Worn = 0; Worn < 3; ++Worn)
				{
					ClassesWorn = static_cast<uint16>(ClassesWorn | Class(EquipList.SavedSlots[10 + Worn]));
				}
			}
		}
		else
		{
			Kind = Which + 1;
			const uint8 Weapon = EquipList.SavedSlots[static_cast<std::size_t>(Which)];
			Gun = bGear ? GearWeapon(Weapon) : Tables.Weapon(Weapon);
		}
		std::fill(EquipList.Candidates.begin(), EquipList.Candidates.end(), uint8{0});
		std::fill(EquipList.CandidateCounts.begin(), EquipList.CandidateCounts.end(), uint8{0});

		// An accessory's list starts with an empty entry: taking it off.
		std::size_t Listed = Kind == AccessoryKind ? 1 : 0;
		const int32 List = bGear ? (Kind == AccessoryKind ? 4 : 3) : (Kind == AccessoryKind ? 2 : 1);
		const std::span<const uint8> Items = Game.InventoryItems(List);
		const std::span<const uint8> Counts = Game.InventoryCounts(List);
		for (std::size_t At = 0; At < Slots && At < Items.size(); ++At)
		{
			const uint8 Id = Items[At];
			bool bTakes = false;
			if (!bGear)
			{
				const std::span<const uint8> Weapon = Tables.Weapon(Id);
				if (Kind == AccessoryKind)
				{
					const std::span<const uint8> Accessory = Tables.Accessory(Id);
					if (Accessory.size() >= RecordSize && (ReadU16(Accessory, 0) >> Character & 1) != 0)
					{
						const uint16 Class = ReadU16(Accessory, 0x0E);
						bTakes = Class == 0 || (ClassHere & Class) != 0 || (ClassesWorn & Class) == 0;
					}
				}
				else if (Weapon.size() >= RecordSize && (ReadU16(Weapon, 0) >> Character & 1) != 0)
				{
					bTakes = Kind == WeaponKind ? Weapon[6] < WeaponTypes && Id < FirstAmmunition
												: Gun.size() > 6 && Weapon[6] == Gun[6] && Id >= FirstAmmunition;
				}
			}
			else
			{
				const auto Allows = [Gear](uint32 Mask) { return Gear < 32 && (Mask >> Gear & 1) != 0; };
				if (Kind == AccessoryKind)
				{
					const std::span<const uint8> Part = PartTables.Part(Id);
					if (Part.size() >= 0x0A && Allows(ReadU32(Part, 0)))
					{
						const uint16 Class = ReadU16(Part, 8);
						bTakes = Class == 0 || (ClassHere & Class) != 0 || (ClassesWorn & Class) == 0;
					}
				}
				else
				{
					const std::span<const uint8> Weapon = GearWeapon(Id);
					if (Weapon.size() >= 0x10 && Allows(ReadU32(Weapon, 4)))
					{
						bTakes = Kind == WeaponKind ? Weapon[0x0F] < WeaponTypes && Id < FirstAmmunition
													: Gun.size() > 0x0F && Weapon[0x0F] == Gun[0x0F] && Id >= FirstAmmunition;
					}
				}
			}
			if (bTakes && Listed < EquipList.Candidates.size())
			{
				EquipList.Candidates[Listed] = Id;
				EquipList.CandidateCounts[Listed] = Counts[At];
				++Listed;
			}
		}

		// The rows shown: a name and a count each, typed a pair of planes at a time.
		std::fill(NameImage.Units.begin(), NameImage.Units.end(), uint16{0});
		for (int32 Row = 0; Row < ListRows; ++Row)
		{
			const std::size_t At = static_cast<std::size_t>(Row);
			const uint8 Id = EquipList.Candidates[static_cast<std::size_t>(Top + Row)];
			if (Id == 0)
			{
				EquipList.RowShown[At] = false;
				continue;
			}
			const int32 NameTable = bGear ? (Kind == AccessoryKind ? GearPartNames : GearWeaponNames) : (Kind == AccessoryKind ? AccessoryNames : WeaponNames);
			FMenuLabel& Name = EquipList.Names[At];
			FMenuLabel& Count = EquipList.Counts[At];
			Name.Width = static_cast<uint8>(RenderString(NameTyping, NameImage, Services.Text.Tables->String(NameTable, Id), 0));
			const uint8 Held = EquipList.CandidateCounts[static_cast<std::size_t>(Top + Row)];
			const std::array<uint16, 2> Glyphs = {static_cast<uint16>(Held / 10 == 0 ? BlankGlyph : Held / 10 + FirstDigitGlyph),
				static_cast<uint16>(Held % 10 + FirstDigitGlyph)};
			GlyphText(Glyphs, GlyphScratch);
			Count.Width = static_cast<uint8>(RenderString(NameTyping, NameImage, GlyphScratch, 1));
			NameImage.Rect = {static_cast<int16>(StripX + (Row & 1) * StripColumn), static_cast<int16>(StripY + Row / 2 * RowHeight), StripWidth,
				static_cast<int16>(RowHeight)};
			Upload(NameImage);
			SetStripLabelTexture(Name, Row, StripBase, NamePlane);
			SetStripLabelTexture(Count, Row, StripBase, CountPlane);
			SetLayerQuad(Name.Quad.Points, NameX, FirstRowY + RowHeight * Row, Name.Width, RowHeight);
			SetLayerQuad(Count.Quad.Points, CountX, FirstRowY + RowHeight * Row, Count.Width, RowHeight);
			RecordLabel(Name);
			RecordLabel(Count);
			EquipList.RowShown[At] = true;
		}
		DrawMemberName(EquipList.MemberName, Slot, 0, bGear);
		Flags[MenuFlag::EquipList] = 1;
		return Listed < static_cast<std::size_t>(ListRows) ? 0 : static_cast<int32>(Listed) - ListRows;
	}

	void FMenuMain::DrawEquipSlots(int32 Page)
	{
		ClearFlags(MenuFlag::EquipSlotLabels, static_cast<int32>(EquipSlotLabels.size()));
		const FMenuMainTables& Tables = Resources.Main();
		const int32 First = Page == 0 ? 0 : 2;
		for (int32 Label = First; Label < First + SlotLabelCount(Page); ++Label)
		{
			FMenuLabel& Placed = EquipSlotLabels[static_cast<std::size_t>(Label)];
			SetLayerQuad(Placed.Quad.Points, 0x18, Tables.SlotLabelY[static_cast<std::size_t>(Label)], Placed.Width, RowHeight);
			RecordLabel(Placed);
			Flags[static_cast<std::size_t>(MenuFlag::EquipSlotLabels + Label)] = 1;
		}
		if (Flags[MenuFlag::Windows + 4] != 0)
		{
			CloseWindow(4);
		}
		OpenWindow(4, 0x10, 0x0C, 0x80, static_cast<uint16>(Page == 0 ? 0x5A : 0x72), false, true, PiecesEntry, false);
	}

	void FMenuMain::DrawEquipDescription(int32 Which, int32 Row, int32 Top, int32 Page, bool bGear, bool bWorn, int32 Slot)
	{
		// The text of the candidate highlighted, or of what the slot wears.
		uint8 Id = bWorn ? 0xFF : EquipList.Candidates[static_cast<std::size_t>(Top + Row)];
		if (Id == 0)
		{
			EquipList.bDescriptionShown = false;
			return;
		}
		const bool bAccessory = Page == 0 && Which != 0;
		using EText = FMenuResources::EText;
		const EText Text = bGear ? (bAccessory ? EText::GearPartHelp : EText::GearWeaponHelp) : (bAccessory ? EText::AccessoryHelp : EText::WeaponHelp);
		if (bWorn)
		{
			const std::size_t At = static_cast<std::size_t>(Which);
			if (Page != 0)
			{
				Id = RecordByte(Slot, bGear, (bGear ? 0x04 : 0x6F) + At);
			}
			else if (Which == 0)
			{
				Id = RecordByte(Slot, bGear, bGear ? 0x0C : 0x6A);
			}
			else
			{
				Id = RecordByte(Slot, bGear, (bGear ? 0x09 : 0x74) + At - 1);
			}
		}
		if (Id == 0)
		{
			EquipList.bDescriptionShown = false;
			return;
		}
		std::fill(NameImage.Units.begin(), NameImage.Units.end(), uint16{0});
		for (int32 Line = 0; Line < DescriptionLines; ++Line)
		{
			FMenuLabel& Label = EquipList.Description[static_cast<std::size_t>(Line)];
			Label.Width = static_cast<uint8>(RenderString(NameTyping, NameImage, Resources.Text(Text).String(Id * 3 + Line), 0));
			const int32 Strip = Line + DescriptionStrip;
			NameImage.Rect = {static_cast<int16>(StripX + (Strip & 1) * StripColumn), static_cast<int16>(StripY + Strip / 2 * RowHeight), StripWidth,
				static_cast<int16>(RowHeight)};
			Upload(NameImage);
			SetStripLabelTexture(Label, Strip, StripBase, NamePlane);
			SetLayerQuad(Label.Quad.Points, DescriptionX, (Line * 16 + DescriptionY) & 0xFFFE, Label.Width, RowHeight);
			RecordLabel(Label);
		}
		EquipList.bDescriptionShown = true;
	}

	void FMenuMain::AddEquipList()
	{
		if (Flags[MenuFlag::EquipList] == 0)
		{
			return;
		}
		for (std::size_t Row = 0; Row < EquipList.RowShown.size(); ++Row)
		{
			if (EquipList.RowShown[Row])
			{
				AddProjected(EquipList.Names[Row].Quad, true, PiecesEntry);
				AddProjected(EquipList.Counts[Row].Quad, true, PiecesEntry);
			}
		}
		AddProjected(EquipList.MemberName, true, PiecesEntry);
		if (EquipList.bDescriptionShown)
		{
			for (FMenuLabel& Line : EquipList.Description)
			{
				AddProjected(Line.Quad, true, PiecesEntry);
			}
		}
	}
}
