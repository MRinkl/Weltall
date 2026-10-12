#include "Menu/MenuButtonIcons.h"
#include "Menu/MenuDrawing.h"
#include "Menu/MenuMesWindow.h"
#include "Support/BoundGlyphs.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Text/MesGlyphs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <string_view>

using namespace Weltall;

// The buttons a menu-style list's text shows: a kernel window's rows as the arena's tutorial and a battle's event
// draw them (a row's two pages apart in the list), through the menus' draw builder to the player's pictures.

namespace
{
	constexpr int32 PagePixels = 256;

	std::vector<uint8> MesString(std::string_view Words)
	{
		std::vector<uint8> String;
		for (const char Character : Words)
		{
			// '*' stands for the Cross button's glyph.
			String.push_back(Character == '*' ? uint8{0x7F} : MesCodeOfCharacter(Character)->Code);
		}
		String.push_back(0x00);
		return String;
	}

	FMenuPrimitive TextSprite(int16 X, int16 Y, int16 Width)
	{
		FMenuPrimitive Sprite;
		Sprite.Code = MenuGpu::Sprite | MenuGpu::RawTexture;
		Sprite.Corners[0].X = X;
		Sprite.Corners[0].Y = Y;
		Sprite.Width = Width;
		Sprite.Height = 13;
		return Sprite;
	}

	FMenuPrimitive PageMode(int32 VramX)
	{
		FMenuPrimitive Mode;
		Mode.Code = MenuGpu::DrawModeCommand;
		Mode.Words = {MenuGpu::DrawMode(MenuGpu::TexturePage(0, 0, VramX, 0x100)), 0};
		return Mode;
	}
}

TEST_CASE("A kernel window's rows tell the buttons they show, a row's second page named with its first", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	struct FNoNames final : IMesGameState
	{
		std::span<const uint8> Name(int32) const override { return {}; }
		uint8 PartyName(int32) const override { return 0xFF; }
	} State;
	FMesContext Text{&Resident.Font, &Resident.NameTables, &State, {1, 0, 3, 2, 4, 5, 6, 7}, {}};
	FMesWindow Window(79, 2);
	Window.Queue(MesString("Hit * and keep hitting the L1 button for as long as you can"));
	for (int32 Tick = 0; Tick < 400; ++Tick)
	{
		Window.Update(Text);
	}
	// One row, wider than a page of the window's image.
	REQUIRE(Window.Row(0).WidthUnits * 4 > PagePixels);
	const FMesWindowPlace Place{0x380, 0x100, 24, 40, 0x7C10, 0x7C50};
	std::vector<FMenuPrimitive> Plain;
	AppendMesWindowPrimitives(Window, Place, Plain);
	std::vector<FMenuPrimitive> Out;
	FMesButtonCells Finder;
	std::vector<FTextButtonCell> Cells;
	AppendMesWindowPrimitives(Window, Place, Out, {&Finder, &Resident.Font, &Cells});
	// The window is drawn as it is without the buttons asked for.
	REQUIRE(Out.size() == Plain.size());

	REQUIRE(Cells.size() == 2);
	const auto& Glyphs = Window.Row(0).Glyphs;
	const auto Cross = std::find_if(Glyphs.begin(), Glyphs.end(), [](const FMesPlacedGlyph& Glyph) { return Glyph.Code.Code == 0x7F; });
	REQUIRE(Cross != Glyphs.end());
	const FTextButtonCell& Glyph = Cells[0];
	CHECK(Glyph.Button == EPadButton::Cross);
	CHECK(Glyph.OffsetX == Cross->XUnits * 4);
	CHECK(Glyph.Width == 12);
	CHECK(Cells[1].Button == EPadButton::L1);
	CHECK(Cells[1].Primitive == Glyph.Primitive);
	// Both lie in the row's first page, whose sprite starts the text; the second page's is named as the rest of their line.
	const FMenuPrimitive& First = Out[Glyph.Primitive];
	CHECK(First.IsRectangle());
	CHECK(First.Corners[0].X == Place.ScreenX);
	CHECK(First.Width == PagePixels);
	REQUIRE(Glyph.LineRest != 0);
	REQUIRE(Glyph.LineRest - 1 < Out.size());
	const FMenuPrimitive& Second = Out[Glyph.LineRest - 1];
	CHECK(Second.IsRectangle());
	CHECK(Second.Corners[0].X == Place.ScreenX + PagePixels);
	CHECK(Second.Corners[0].Y == First.Corners[0].Y);
	CHECK(Cells[1].LineRest == Glyph.LineRest);
}

TEST_CASE("The menus' builder hands a frame's text buttons on as cells of the list it builds", "[Menu]")
{
	// As a window draws a row of two pages: each page's drawing mode, then its sprite.
	FMenuFrame Frame;
	Frame.Primitives = {PageMode(0x380), TextSprite(24, 40, 256), PageMode(0x3C0), TextSprite(280, 40, 20)};
	FTextButtonCell Cell;
	Cell.Primitive = 1;
	Cell.OffsetX = 16;
	Cell.Button = EPadButton::Cross;
	Cell.LineRest = 4;
	Frame.TextButtons = {Cell};
	FMenuDrawBuilder Builder;
	FDrawList List;
	Builder.Build(Frame, nullptr, 1.0f, List);
	// The drawing modes draw nothing: the two sprites are the list's first two.
	REQUIRE(List.Primitives.size() == 2);
	REQUIRE(Builder.TextCells.size() == 1);
	CHECK(Builder.TextCells[0].Primitive == 0);
	CHECK(Builder.TextCells[0].LineRest == 2);
	CHECK(Builder.TextCells[0].OffsetX == 16);
	CHECK(Builder.TextCells[0].Button == EPadButton::Cross);

	// A frame without any leaves none behind.
	Frame.TextButtons.clear();
	Builder.Build(Frame, nullptr, 1.0f, List);
	CHECK(Builder.TextCells.empty());
}

TEST_CASE("What follows a button in its line moves with its picture, the line's sprite apart in the list too", "[Menu]")
{
	// Two rows of text, each 256 pixels and then 40 more: the first pages, then the second pages, as a window draws them.
	const FDrawTexture Page{0, FTexturePage{0x380, 0x100, ETextureDepth::Indexed4, EBlendMode::Average}, {}};
	const auto Text = [&Page]
	{
		FDrawList List;
		List.Primitives.push_back(MakeSprite(24, 54, 256, 13, Page, 0, 13));
		List.Primitives.push_back(MakeSprite(24, 40, 256, 13, Page, 0, 0));
		List.Primitives.push_back(MakeSprite(280, 54, 40, 13, Page, 0, 13));
		List.Primitives.push_back(MakeSprite(280, 40, 40, 13, Page, 0, 0));
		return List;
	};
	// The upper row: "L1" in words 100 pixels into its first page, 30 wide, and a glyph 8 into its second. The lower
	// row: "R1" in words at its start.
	FTextButtonCell Upper;
	Upper.Primitive = 1;
	Upper.OffsetX = 100;
	Upper.Width = 30;
	Upper.Button = EPadButton::L1;
	Upper.LineRest = 4;
	FTextButtonCell UpperRest;
	UpperRest.Primitive = 3;
	UpperRest.OffsetX = 8;
	UpperRest.Button = EPadButton::Square;
	FTextButtonCell Lower;
	Lower.Primitive = 0;
	Lower.Width = 30;
	Lower.Button = EPadButton::R1;
	Lower.LineRest = 3;
	const std::array<FTextButtonCell, 3> Cells{Upper, UpperRest, Lower};

	// A picture is narrower than a button's name: what follows closes up.
	Testing::FBoundGlyphs Xbox(EButtonIconSet::Xbox);
	Xbox.Bind({{EPadButton::L1, "LB"}, {EPadButton::R1, "RB"}, {EPadButton::Square, "X"}});
	FDrawList List = Text();
	FMenuButtonIcons Pictures;
	Pictures.ApplyToText(List, Cells, Xbox);
	// Lower row: picture, rest. Upper row: left, picture, rest. Lower second page whole. Upper second page: left, picture, rest.
	REQUIRE(List.Primitives.size() == 9);
	const auto Left = [&List](std::size_t Index) { return List.Primitives[Index].Corners[0].X; };
	const auto Right = [&List](std::size_t Index) { return List.Primitives[Index].Corners[1].X; };
	const auto IsPicture = [&List](std::size_t Index) { return List.Primitives[Index].Texture->Vram == FMenuButtonIcons::PicturesVram; };
	CHECK(IsPicture(0));
	const float LowerShift = Left(1) - 54.0f;
	CHECK(LowerShift < 0.0f);
	CHECK(Right(1) == 280.0f + LowerShift);
	CHECK(Left(2) == 24.0f);
	CHECK(Right(2) == 124.0f);
	CHECK(IsPicture(3));
	const float UpperShift = Left(4) - 154.0f;
	CHECK(UpperShift < 0.0f);
	CHECK(Right(4) == 280.0f + UpperShift);
	// The lower row's second page follows its first page's button.
	CHECK_FALSE(IsPicture(5));
	CHECK(Left(5) == 280.0f + LowerShift);
	CHECK(List.Primitives[5].Corners[0].Y == 54.0f);
	// The upper row's second page, cut about its own button, follows the first page's: every piece of it.
	CHECK(Left(6) == 280.0f + UpperShift);
	CHECK(Right(6) == 288.0f + UpperShift);
	CHECK(IsPicture(7));
	CHECK(Left(7) == 288.0f + UpperShift);
	CHECK(Left(8) == 300.0f + UpperShift);
	CHECK(List.Primitives[8].Corners[0].Y == 40.0f);

	// The second page keeps its place where the first shows the text as it is.
	Testing::FBoundGlyphs PlayStation(EButtonIconSet::PlayStation);
	PlayStation.Bind({{EPadButton::L1, "L1"}, {EPadButton::R1, "R1"}, {EPadButton::Square, "Square"}});
	FDrawList Kept = Text();
	Pictures.ApplyToText(Kept, Cells, PlayStation);
	REQUIRE(Kept.Primitives.size() == 4);
	CHECK(Kept.Primitives[3].Corners[0].X == 280.0f);
}

TEST_CASE("A window's row names a button whose word starts the next row, typed or still to be typed", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	struct FNoNames final : IMesGameState
	{
		std::span<const uint8> Name(int32) const override { return {}; }
		uint8 PartyName(int32) const override { return 0xFF; }
	} State;
	FMesContext Text{&Resident.Font, &Resident.NameTables, &State, {1, 0, 3, 2, 4, 5, 6, 7}, {}};
	// The arena's Informationist on guarding: "That's why we have the L1 / button to GUARD you!"
	std::vector<uint8> String = MesString("That's why we have the L1 ");
	String.back() = 0x01;
	const std::vector<uint8> Next = MesString("button to GUARD you!");
	String.insert(String.end(), Next.begin(), Next.end());
	// Narrower than the 100 units a line's end moves the pen to, as the game's windows are.
	FMesWindow Window(95, 2);
	Window.Queue(String);
	constexpr std::size_t NameTyped = 25;
	const auto NamesGuard = [](const std::vector<FTextButtonCell>& Cells)
	{
		return std::any_of(Cells.begin(), Cells.end(), [](const FTextButtonCell& Cell) { return Cell.Button == EPadButton::L1; });
	};
	FMesButtonCells Finder;
	std::vector<FTextButtonCell> Cells;
	bool bNamedWhileTyping = false;
	for (int32 Tick = 0; Tick < 400; ++Tick)
	{
		Window.Update(Text);
		if (Window.CursorRow() == 0 && Window.Row(0).Glyphs.size() >= NameTyped)
		{
			Cells.clear();
			Finder.Add(Window, 0, 0, 1024, &Resident.Font, 0, Cells);
			bNamedWhileTyping = bNamedWhileTyping || NamesGuard(Cells);
		}
	}
	// While its row still typed, the name's word was read on in the string; typed, it is read in the row below.
	CHECK(bNamedWhileTyping);
	REQUIRE(Window.CursorRow() == 1);
	Cells.clear();
	Finder.Add(Window, 0, 0, 1024, &Resident.Font, 0, Cells);
	REQUIRE(Cells.size() == 1);
	CHECK(Cells[0].Button == EPadButton::L1);
	CHECK(Cells[0].OffsetX == Window.Row(0).Glyphs[NameTyped - 2].XUnits * 4);
	Cells.clear();
	Finder.Add(Window, 1, 0, 1024, &Resident.Font, 0, Cells);
	CHECK(Cells.empty());
}
