#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Game/GameStateLayout.h"
#include "Menu/MenuMain.h"
#include "Menu/MenuResources.h"

namespace Weltall
{
	namespace
	{
		constexpr int32 FirstPortraitFrame = 0x14B;
		constexpr int32 FullScale = 0x1000;
		/** A slot's panel lies this many rows below the one before. */
		constexpr int32 PanelStep = 0x38;
		constexpr int32 PanelLabels = 9;
		/** The table's places after the labels: portrait, level, ether level, HP, max HP, EP, max EP, name. */
		namespace Place
		{
			constexpr std::size_t Portrait = 9;
			constexpr std::size_t Level = 10;
			constexpr std::size_t Hp = 12;
			constexpr std::size_t MaxHp = 13;
			constexpr std::size_t Ep = 14;
			constexpr std::size_t MaxEp = 15;
			constexpr std::size_t Name = 16;
		}
		/** The pointer beside a panel. */
		constexpr int16 PointerLeft = 0x90;
		constexpr int16 PointerRight = 0xA0;
		constexpr int16 PointerTop = 0x30;
		constexpr int16 PointerHeight = 0x10;
		constexpr int32 NamePageX = 0x180;
		constexpr int32 NameWidth = 0x48;
		constexpr int32 GearNameExtra = 0x18;
		constexpr int32 NameRows = 13;
		constexpr int32 TextClutY = 0x1D1;
		constexpr int32 OddClutX = 16;
		constexpr int32 DigitWidth = 8;
		constexpr uint8 Blank = 0xFF;
		constexpr uint8 Bright = 0x80;
		constexpr uint8 Dim = 0x21;
		constexpr int32 PiecesEntry = 4;
		/** Links beyond a panel's labels, from its tenth part: the original links five of them again. */
		constexpr std::size_t ExtraLabelParts = 5;
	}

	void FMenuMain::ShadePiece(FMenuPrimitive& Piece, int32 Mode)
	{
		Piece.Code = static_cast<uint8>(Piece.Code & ~MenuGpu::RawTexture);
		switch (Mode)
		{
		case 0:
			Piece.Code = static_cast<uint8>(Piece.Code & ~MenuGpu::SemiTransparent);
			Piece.Corners[0].Color = {Bright, Bright, Bright};
			break;
		case 1:
			Piece.Page = static_cast<uint16>(Piece.Page | MenuGpu::PageBlendAdd);
			Piece.Code = static_cast<uint8>(Piece.Code | MenuGpu::SemiTransparent);
			Piece.Corners[0].Color = {Dim, Dim, Dim};
			break;
		case 2:
			Piece.Corners[0].Color = {Bright, Bright, Bright};
			break;
		case 3:
			Piece.Corners[0].Color = {Dim, Dim, Dim};
			break;
		default:
			break;
		}
	}

	void FMenuMain::DimWindow(int32 Index, bool bDim)
	{
		FMenuWindow& Window = Windows[static_cast<std::size_t>(Index)];
		const int32 Mode = bDim ? 3 : 2;
		for (FMenuPrimitive& Piece : Window.Corners)
		{
			ShadePiece(Piece, Mode);
		}
		for (FMenuPrimitive& Piece : Window.Edges)
		{
			ShadePiece(Piece, Mode);
		}
		for (FMenuPrimitive& Piece : Window.ScrollBar)
		{
			ShadePiece(Piece, Mode);
		}
	}

	void FMenuMain::DrawTargets(int32 Kind)
	{
		const bool bGear = Kind == 2;
		bTargetPanelsMade = true;
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			FMenuTargetPanel& Panel = TargetPanels[static_cast<std::size_t>(Slot)];
			Panel = {};
			const uint8 Character = Member(Slot);
			if (Character == FGameState::EmptySlot)
			{
				continue;
			}
			if (!bGear || Services.Game->CharacterGear(Character).value_or(FGameState::EmptySlot) != FGameState::EmptySlot)
			{
				BuildTargetPanel(Panel, Character, Slot, bGear);
			}
		}
		Flags[MenuFlag::TargetPanels] = 1;
		for (int32 Placed = 0; Placed < 3; ++Placed)
		{
			std::vector<FMenuPrimitive>& Shown = TargetPointers[static_cast<std::size_t>(Placed)];
			if (Shown.empty())
			{
				continue;
			}
			const int16 Top = static_cast<int16>(PointerTop + PanelStep * Placed);
			FMenuPrimitive& Quad = Shown.front();
			Quad.Corners[0].X = PointerLeft;
			Quad.Corners[0].Y = Top;
			Quad.Corners[1].X = PointerRight;
			Quad.Corners[1].Y = Top;
			Quad.Corners[2].X = PointerLeft;
			Quad.Corners[2].Y = static_cast<int16>(Top + PointerHeight);
			Quad.Corners[3].X = PointerRight;
			Quad.Corners[3].Y = static_cast<int16>(Top + PointerHeight);
		}
	}

	void FMenuMain::BuildTargetPanel(FMenuTargetPanel& Panel, uint8 Character, int32 Slot, bool bGear)
	{
		using namespace GameStateLayout;
		const FMenuMainTables& Tables = Resources.Main();
		const FMenuSpriteSheet& Sheet = Resources.Sheet();
		const std::size_t Kind = bGear ? 1 : 0;
		const std::array<int32, 17>& X = Tables.TargetX[Kind];
		const std::array<int32, 17>& Y = Tables.TargetY[Kind];
		const int32 Down = Slot * PanelStep;
		for (int32 Label = 0; Label < PanelLabels; ++Label)
		{
			const int32 Frame = Tables.TargetLabels[Kind * PanelLabels + static_cast<std::size_t>(Label)];
			if (Frame != FMenuMainTables::NoCommand)
			{
				Sheet.Draw(Frame, X[static_cast<std::size_t>(Label)], Down + Y[static_cast<std::size_t>(Label)], FullScale, Panel.Labels);
			}
		}
		Scratch.clear();
		Sheet.Draw(FirstPortraitFrame + Slot, X[Place::Portrait], Down + Y[Place::Portrait], FullScale, Scratch);
		if (!Scratch.empty())
		{
			Panel.Portrait = Scratch.front();
		}

		// The name, from its strip: a character's, or its gear's (wider).
		const uint8 Gear = Services.Game->CharacterGear(Character).value_or(0);
		FMenuPrimitive& Name = Panel.Name;
		Name.Code = MenuGpu::TexturedQuad;
		Name.Corners[0].Color = {Bright, Bright, Bright};
		Name.Page = MenuGpu::TexturePage(0, 0, NamePageX, 0);
		const bool bOdd = bGear ? (Gear & 1) == 0 : (Character & 1) != 0;
		Name.Clut = MenuGpu::Clut(bOdd ? OddClutX : 0, TextClutY);
		const std::size_t Strip = Kind * 3 + static_cast<std::size_t>(Slot);
		const int32 Width = NameWidth + (bGear ? GearNameExtra : 0);
		const int16 Left = static_cast<int16>(X[Place::Name]);
		const int16 Top = static_cast<int16>(Y[Place::Name] + Down);
		Name.Corners[0].X = Left;
		Name.Corners[0].Y = Top;
		Name.Corners[1].X = static_cast<int16>(Left + Width);
		Name.Corners[1].Y = Top;
		Name.Corners[2].X = Left;
		Name.Corners[2].Y = static_cast<int16>(Top + NameRows);
		Name.Corners[3].X = static_cast<int16>(Left + Width);
		Name.Corners[3].Y = static_cast<int16>(Top + NameRows);
		SetTexelRect(Name, (Tables.NameStripX[Strip] & 0x3F) << 2, Tables.NameStripY[Strip], Width, NameRows);

		// The numbers: the level and HP right-aligned, the maxima packed; a gear shows HP alone, five digits.
		const std::span<const uint8> Record = Services.Game->Block().subspan(CharacterOffset(Character), CharacterSize);
		const auto Numbers = [&](std::vector<FMenuPrimitive>& Digits, uint32 Value, int32 First, int32 Count, std::size_t At, bool bPacked)
		{
			SplitNumber(Value);
			int32 Placed = 0;
			for (int32 Digit = 0; Digit < Count; ++Digit)
			{
				const uint8 Frame = NumberDigits[static_cast<std::size_t>(First + Digit)];
				if (Frame != Blank)
				{
					Sheet.Draw(Frame, X[At] + (bPacked ? Placed : Digit) * DigitWidth, Down + Y[At], FullScale, Digits);
					++Placed;
				}
			}
		};
		Numbers(Panel.Level, Record[Character::Level], 6, 3, Place::Level, false);
		if (!bGear)
		{
			Numbers(Panel.Hp, LittleEndian::ReadU16(Record, Character::Hp), 6, 3, Place::Hp, false);
			Numbers(Panel.MaxHp, LittleEndian::ReadU16(Record, Character::MaxHp), 6, 3, Place::MaxHp, true);
			Numbers(Panel.Ep, LittleEndian::ReadU16(Record, Character::Ep), 7, 2, Place::Ep, false);
			Numbers(Panel.MaxEp, LittleEndian::ReadU16(Record, Character::MaxEp), 7, 2, Place::MaxEp, true);
		}
		else
		{
			const std::span<const uint8> GearRecord = Services.Game->Block().subspan(GearOffset(Gear), GearSize);
			Numbers(Panel.Hp, LittleEndian::ReadU32(GearRecord, GameStateLayout::Gear::Hp), 4, 5, Place::Hp, false);
			Numbers(Panel.MaxHp, LittleEndian::ReadU32(GearRecord, GameStateLayout::Gear::MaxHp), 4, 5, Place::MaxHp, true);
		}
		Panel.bShown = true;
	}

	void FMenuMain::AddTargetPanels()
	{
		if (Flags[MenuFlag::TargetPanels] == 0)
		{
			return;
		}
		for (FMenuTargetPanel& Panel : TargetPanels)
		{
			if (!Panel.bShown)
			{
				continue;
			}
			Table.Add(PiecesEntry, Panel.Portrait);
			Table.Add(PiecesEntry, Panel.Name);
			AddScreen(Panel.Labels);
			AddScreen(Panel.Level);
			AddScreen(Panel.Hp);
			AddScreen(Panel.MaxHp);
			AddScreen(Panel.Ep);
			AddScreen(Panel.MaxEp);
			for (std::size_t Part = PanelLabels; Part < PanelLabels + ExtraLabelParts && Part < Panel.Labels.size(); ++Part)
			{
				Table.Add(PiecesEntry, Panel.Labels[Part]);
			}
		}
	}
}
