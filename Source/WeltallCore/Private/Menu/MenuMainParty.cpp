#include "Game/GameState.h"
#include "Game/GameStateLayout.h"
#include "Foundation/LittleEndian.h"
#include "Menu/MenuMain.h"

namespace Weltall
{
	namespace
	{
		/** Where the panels slide in from, off the screen's right. */
		constexpr std::array<std::array<int32, 2>, 3> PanelEntries = {{{0x100, 0x86}, {0x108, 0x3E}, {0x110, -10}}};
		constexpr int32 SlideSteps = 8;
		constexpr int32 StepUnit = 0x100;
		constexpr int32 FirstPortraitFrame = 0x14B;
		constexpr int32 PiecesEntry = 4;
		constexpr int32 FullScale = 0x1000;
		constexpr int32 DigitWidth = 8;
		constexpr int32 ArcLabelsBase = 4;
		/** A panel's name, rendered with the pair of names it belongs to into the page at (384, 0). */
		constexpr int32 NamePageX = 0x180;
		constexpr int32 NameWidth = 0x48;
		constexpr int32 NameRows = 13;
		constexpr int32 TextClutY = 0x1D1;
		constexpr int32 OddClutX = 16;
		constexpr int32 NoLabel = 0xFFFF;
		/** The ether level's digits are tinted green. */
		constexpr FDrawColor EtherGreen{0, 0x80, 0};
		constexpr uint8 Blank = 0xFF;

		namespace Number
		{
			constexpr std::size_t Level = 0;
			constexpr std::size_t EtherLevel = 1;
			constexpr std::size_t Hp = 2;
			constexpr std::size_t MaxHp = 3;
			constexpr std::size_t Ep = 4;
			constexpr std::size_t MaxEp = 5;
			constexpr std::size_t Next = 6;
			constexpr std::size_t EtherNext = 7;
		}

		namespace Effect
		{
			constexpr uint8 PanelsArrive = 0x5D;
		}

		/** A slide's distance so far in whole pixels, rounded toward zero as the original's shift after its bias. */
		int32 Whole(int32 Moved)
		{
			return (Moved < 0 ? Moved + 0xFF : Moved) >> 8;
		}
	}

	void FMenuMain::StartSlide(int32 FromX, int32 FromY, int32 ToX, int32 ToY, int32 Steps, int32 Slot)
	{
		FMenuSlide& Slide = Slides[static_cast<std::size_t>(Slot)];
		Slide.FromX = FromX;
		Slide.FromY = FromY;
		Slide.ToX = ToX;
		Slide.ToY = ToY;
		Slide.bLeftward = ToX < FromX;
		Slide.bUpward = ToY < FromY;
		const int32 Across = Slide.bLeftward ? FromX - ToX : ToX - FromX;
		const int32 Down = Slide.bUpward ? FromY - ToY : ToY - FromY;
		// The longer axis moves a pixel a step, the other in proportion.
		if (Across < Down)
		{
			Slide.StepY = StepUnit;
			Slide.StepX = (Across << 8) / Down;
		}
		else
		{
			Slide.StepX = StepUnit;
			Slide.StepY = (Down << 8) / Across;
		}
		Slide.Steps = Steps;
		Slide.MovedX = 0;
		Slide.MovedY = 0;
		Slide.bDone = false;
	}

	void FMenuMain::StepSlide(int32 Slot)
	{
		FMenuSlide& Slide = Slides[static_cast<std::size_t>(Slot)];
		for (int32 Step = 0; Step < Slide.Steps; ++Step)
		{
			Slide.MovedX += Slide.bLeftward ? -Slide.StepX : Slide.StepX;
			Slide.MovedY += Slide.bUpward ? -Slide.StepY : Slide.StepY;
		}
		// Done once the longer axis has gone past where the slide ends.
		const bool bAcross = Slide.StepX == StepUnit;
		const int32 At = bAcross ? Whole(Slide.MovedX) + Slide.FromX : Whole(Slide.MovedY) + Slide.FromY;
		const int32 To = bAcross ? Slide.ToX : Slide.ToY;
		const bool bBackward = bAcross ? Slide.bLeftward : Slide.bUpward;
		if (bBackward ? At < To : At > To)
		{
			Slide.bDone = true;
		}
	}

	TMenuTask<> FMenuMain::SlideParty(bool bIn, bool bKeepsGold)
	{
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			const std::array<int32, 2>& Place = PanelPlaces[static_cast<std::size_t>(Slot)];
			const std::array<int32, 2>& Entry = PanelEntries[static_cast<std::size_t>(Slot)];
			if (bIn)
			{
				StartSlide(Entry[0], Entry[1], Place[0], Place[1], SlideSteps, Slot);
			}
			else
			{
				StartSlide(Place[0], Place[1], Entry[0], Entry[1], SlideSteps, Slot);
			}
		}
		if (bIn)
		{
			RenderLabels(ArcLabels, Resources.Main().MainLabels, ArcLabelsBase);
		}
		else
		{
			ClearFlags(MenuFlag::ArcLabels, static_cast<int32>(ArcLabels.size()));
		}
		// Every member's panel moves until the first two slots' slides are over (or the third's).
		if (!Slides[0].bDone && !Slides[1].bDone)
		{
			while (!Slides[2].bDone)
			{
				for (int32 Slot = 0; Slot < 3; ++Slot)
				{
					if (Member(Slot) != FGameState::EmptySlot)
					{
						DrawPartyPanel(Slot, Member(Slot));
					}
				}
				co_await Frame();
				for (int32 Slot = 0; Slot < 3; ++Slot)
				{
					if (Member(Slot) != FGameState::EmptySlot)
					{
						StepSlide(Slot);
					}
				}
				if (Slides[0].bDone || Slides[1].bDone)
				{
					break;
				}
			}
		}
		if (!bIn)
		{
			Flags[0] = 0;
			Flags[1] = 0;
			Flags[2] = 0;
			if (!bKeepsGold)
			{
				Flags[MenuFlag::Windows] = 0;
				Flags[MenuFlag::Gold] = 0;
			}
		}
		else
		{
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				FMenuSlide& Slide = Slides[static_cast<std::size_t>(Slot)];
				Slide.FromX = PanelPlaces[static_cast<std::size_t>(Slot)][0];
				Slide.FromY = PanelPlaces[static_cast<std::size_t>(Slot)][1];
				Slide.MovedX = 0;
				Slide.MovedY = 0;
			}
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				if (Member(Slot) != FGameState::EmptySlot)
				{
					DrawPartyPanel(Slot, Member(Slot));
				}
			}
			PlaySound(Effect::PanelsArrive);
			if (!bKeepsGold)
			{
				ShowGold();
			}
			Flags[MenuFlag::Windows + 1] = 1;
			Flags[MenuFlag::PlayTime] = 1;
		}
		co_await Frame();
	}

	void FMenuMain::DrawDigits(std::vector<FMenuPrimitive>& Digits, uint32 Value, int32 First, int32 Count, int32 X, int32 Y, bool bPacked)
	{
		// Right-aligned in their places, or packed from the left, as each of the panel's numbers is.
		SplitNumber(Value);
		Digits.clear();
		int32 Placed = 0;
		for (int32 Digit = 0; Digit < Count; ++Digit)
		{
			const uint8 Frame = NumberDigits[static_cast<std::size_t>(First + Digit)];
			if (Frame == Blank)
			{
				continue;
			}
			Resources.Sheet().Draw(Frame, X + (bPacked ? Placed : Digit) * DigitWidth, Y, FullScale, Digits);
			++Placed;
		}
	}

	void FMenuMain::DrawPartyPanel(int32 Slot, uint8 Character)
	{
		const FMenuMainTables& Tables = Resources.Main();
		const FMenuSlide& Slide = Slides[static_cast<std::size_t>(Slot)];
		const int32 X = Whole(Slide.MovedX) + Slide.FromX;
		const int32 Y = Whole(Slide.MovedY) + Slide.FromY;
		FMenuPartyPanel& Panel = Panels[static_cast<std::size_t>(Slot)];

		Panel.Portrait.clear();
		Resources.Sheet().Draw(FirstPortraitFrame + Slot, X, Y, FullScale, Panel.Portrait);
		FMenuPrimitive& Name = Panel.Name;
		Name.Code = MenuGpu::TexturedQuad;
		Name.Corners[0].Color = {0x80, 0x80, 0x80};
		Name.Page = MenuGpu::TexturePage(0, 0, NamePageX, 0);
		Name.Clut = MenuGpu::Clut((Character & 1) != 0 ? OddClutX : 0, TextClutY);
		const int16 Left = static_cast<int16>(X + Tables.PanelName[0]);
		const int16 Top = static_cast<int16>(Y + Tables.PanelName[1]);
		const uint8 U = static_cast<uint8>((Tables.NameStripX[static_cast<std::size_t>(Slot)] & 0x3F) << 2);
		const uint8 V = static_cast<uint8>(Tables.NameStripY[static_cast<std::size_t>(Slot)]);
		Name.Corners[0] = {Left, Top, U, V, Name.Corners[0].Color};
		Name.Corners[1] = {static_cast<int16>(Left + NameWidth), Top, static_cast<uint8>(U + NameWidth), V, {}};
		Name.Corners[2] = {Left, static_cast<int16>(Top + NameRows), U, static_cast<uint8>(V + NameRows), {}};
		Name.Corners[3] = {static_cast<int16>(Left + NameWidth), static_cast<int16>(Top + NameRows), static_cast<uint8>(U + NameWidth),
			static_cast<uint8>(V + NameRows), {}};

		Panel.Labels.clear();
		for (std::size_t Label = 0; Label < Tables.PanelLabels.size(); ++Label)
		{
			if (Tables.PanelLabels[Label] != NoLabel)
			{
				Resources.Sheet().Draw(Tables.PanelLabels[Label], X + Tables.PanelLabelX[Label], Y + Tables.PanelLabelY[Label], FullScale, Panel.Labels);
			}
		}

		const FGameState& Game = *Services.Game;
		const std::span<const uint8> Record =
			Game.Block().subspan(GameStateLayout::CharactersOffset + static_cast<std::size_t>(Character) * GameStateLayout::CharacterSize, GameStateLayout::CharacterSize);
		const auto At = [&Tables, X, Y](std::size_t Which, int32 Axis) { return (Axis == 0 ? X : Y) + Tables.PanelNumbers[Which][static_cast<std::size_t>(Axis)]; };
		using LittleEndian::ReadU16;
		using LittleEndian::ReadU32;
		DrawDigits(Panel.Hp, ReadU16(Record, GameStateLayout::Character::Hp), 6, 3, At(Number::Hp, 0), At(Number::Hp, 1), false);
		DrawDigits(Panel.MaxHp, ReadU16(Record, GameStateLayout::Character::MaxHp), 6, 3, At(Number::MaxHp, 0), At(Number::MaxHp, 1), true);
		DrawDigits(Panel.Ep, ReadU16(Record, GameStateLayout::Character::Ep), 7, 2, At(Number::Ep, 0), At(Number::Ep, 1), false);
		DrawDigits(Panel.MaxEp, ReadU16(Record, GameStateLayout::Character::MaxEp), 7, 2, At(Number::MaxEp, 0), At(Number::MaxEp, 1), true);
		DrawDigits(Panel.Next, ReadU32(Record, GameStateLayout::Character::ExpToNext), 2, 7, At(Number::Next, 0), At(Number::Next, 1), false);
		DrawDigits(Panel.EtherNext, ReadU32(Record, GameStateLayout::Character::EtherExpToNext), 2, 7, At(Number::EtherNext, 0), At(Number::EtherNext, 1), false);
		DrawDigits(Panel.Level, Record[GameStateLayout::Character::Level], 6, 3, At(Number::Level, 0), At(Number::Level, 1), false);
		DrawDigits(Panel.EtherLevel, Record[GameStateLayout::Character::EtherLevel], 6, 3, At(Number::EtherLevel, 0), At(Number::EtherLevel, 1), false);
		for (FMenuPrimitive& Digit : Panel.EtherLevel)
		{
			Digit.Code = MenuGpu::TexturedQuad;
			Digit.Corners[0].Color = EtherGreen;
		}
		Flags[static_cast<std::size_t>(Slot)] = 1;
	}

	void FMenuMain::AddPartyPanels()
	{
		// A panel links its portrait's first part, its name, labels, HP, EP, EXP to next and level; its
		// ether level and ether EXP, made green and ready, are never linked.
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			if (Flags[static_cast<std::size_t>(Slot)] == 0)
			{
				continue;
			}
			FMenuPartyPanel& Panel = Panels[static_cast<std::size_t>(Slot)];
			// Keyed by member and part, so a sliding panel slides smoothly on the display, into another slot too.
			const uint32 Key = MenuBlendKey::Panels + static_cast<uint32>(Member(Slot)) * 0x100;
			MenuBlendKey::Set(Panel.Portrait, Key);
			Panel.Name.BlendKey = Key + 0x10;
			MenuBlendKey::Set(Panel.Labels, Key + 0x20);
			MenuBlendKey::Set(Panel.Hp, Key + 0x40);
			MenuBlendKey::Set(Panel.MaxHp, Key + 0x50);
			MenuBlendKey::Set(Panel.Ep, Key + 0x60);
			MenuBlendKey::Set(Panel.MaxEp, Key + 0x70);
			MenuBlendKey::Set(Panel.Next, Key + 0x80);
			MenuBlendKey::Set(Panel.Level, Key + 0x90);
			if (!Panel.Portrait.empty())
			{
				Table.Add(PiecesEntry, Panel.Portrait.front());
			}
			Table.Add(PiecesEntry, Panel.Name);
			AddScreen(Panel.Labels);
			AddScreen(Panel.Hp);
			AddScreen(Panel.MaxHp);
			AddScreen(Panel.Ep);
			AddScreen(Panel.MaxEp);
			AddScreen(Panel.Next);
			AddScreen(Panel.Level);
		}
	}
}
