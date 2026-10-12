#include "Field/EventContext.h"
#include "Field/FieldDrawingTables.h"
#include "Field/FieldRuntime.h"
#include "Field/FieldScreenLayer.h"
#include "Presentation/DrawList.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Text/MesGlyphs.h"
#include "Text/MesWindow.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	FFieldDrawingTables Tables()
	{
		FFieldDrawingTables Result;
		Result.FramePieces = {{{32, 240, 16, 16}, {16, 248, 16, 8}, {48, 240, 16, 16}, {16, 240, 16, 8}, {64, 240, 16, 16}, {0, 240, 8, 16}, {80, 240, 16, 16},
			{8, 240, 8, 16}}};
		for (int32 Frame = 0; Frame < FFieldDrawingTables::CursorFrames; ++Frame)
		{
			Result.ChoiceCursor[static_cast<std::size_t>(Frame)] = {static_cast<int16>(32 + 8 * Frame), 448, 8, 12};
			Result.MoreCursor[static_cast<std::size_t>(Frame)] = {96, static_cast<int16>(448 + 8 * Frame), 12, 8};
		}
		return Result;
	}

	FFieldMessageWindow& OpenWindow(FEventContext& Context, int32 Index, int16 Age)
	{
		FFieldMessageWindow& Window = Context.Messages.Windows[static_cast<std::size_t>(Index)];
		Window.bOpen = true;
		Window.Age = Age;
		Window.Frame.Rect = {40, 100, 96, 44};
		Window.OpenTicks = 3;
		return Window;
	}
}

TEST_CASE("The field's screen layer draws the scripts' fade under the windows and the map's fade over them", "[Field]")
{
	FEventContext Context;
	Context.Render.Fades[0].bActive = true;
	Context.Render.Fades[0].Color = {0x4000, 0x2000, 0x1000};
	Context.Render.Fades[0].BlendMode = 2;
	Context.Render.Fades[1].bActive = true;
	Context.Render.Fades[1].BlendMode = 1;
	OpenWindow(Context, 1, 0);
	FDrawList Layer;
	BuildFieldScreenLayer(Context, Tables(), EFieldFrame::Tick, Layer);
	REQUIRE(Layer.Primitives.size() > 2);
	const FDrawPrimitive& Scripts = Layer.Primitives.front();
	const FDrawPrimitive& Map = Layer.Primitives.back();
	CHECK(Scripts.UntexturedBlend == EBlendMode::Add);
	CHECK(Map.UntexturedBlend == EBlendMode::Subtract);
	CHECK(Map.Blend == EDrawBlend::SemiTransparent);
	CHECK(Map.Corners[0].Color == FDrawColor{0x40, 0x20, 0x10});
	// The fades cover the whole display, however much wider than the 320 x 224 screen.
	CHECK(Map.Corners[0].X < -1000.0f);
	CHECK(Map.Corners[3].X > 1320.0f);
	CHECK(Map.BlendKey != Scripts.BlendKey);
}

TEST_CASE("A message window draws its background box, then its frame of eight pieces repeated along its sides", "[Field]")
{
	FEventContext Context;
	OpenWindow(Context, 0, 0);
	FDrawList Layer;
	BuildFieldScreenLayer(Context, Tables(), EFieldFrame::Tick, Layer);
	REQUIRE_FALSE(Layer.Primitives.empty());
	const FDrawPrimitive& Box = Layer.Primitives.front();
	CHECK_FALSE(Box.Texture);
	CHECK(Box.UntexturedBlend == EBlendMode::Subtract);
	CHECK(Box.Corners[0].X == 40.0f);
	CHECK(Box.Corners[0].Y == 101.0f);
	CHECK(Box.Corners[3].Y == 143.0f);
	// Four corners, the 80-pixel top and bottom edges in 8-texel repeats, the 26-row sides in 8-row ones.
	std::size_t Pieces = 0;
	for (const FDrawPrimitive& Primitive : Layer.Primitives)
	{
		if (Primitive.Texture && Primitive.Texture->Clut == FClutPosition{256, 244})
		{
			CHECK(Primitive.Blend == EDrawBlend::SemiTransparent);
			CHECK(Primitive.Texture->Page.Blend == EBlendMode::Subtract);
			++Pieces;
		}
	}
	CHECK(Pieces == 4 + 2 * 10 + 2 * 4);
	// The top edge's pieces start at the window's left plus 8 and take the frame image's first 8 texels.
	bool bTopEdge = false;
	for (const FDrawPrimitive& Primitive : Layer.Primitives)
	{
		bTopEdge = bTopEdge || (Primitive.Texture && Primitive.Corners[0].X == 48.0f && Primitive.Corners[0].Y == 93.0f && Primitive.Corners[0].U == 0.0f);
	}
	CHECK(bTopEdge);
}

TEST_CASE("The frontmost message window is drawn last, over the older ones", "[Field]")
{
	FEventContext Context;
	OpenWindow(Context, 2, 1).Frame.Rect.X = 10;
	OpenWindow(Context, 3, 0).Frame.Rect.X = 200;
	FDrawList Layer;
	BuildFieldScreenLayer(Context, Tables(), EFieldFrame::Tick, Layer);
	REQUIRE(Layer.Primitives.size() > 2);
	CHECK(Layer.Primitives.front().Corners[0].X == 10.0f);
	CHECK(Layer.Primitives.back().Corners[0].X > 190.0f);
}

TEST_CASE("A message window's line tells where its button glyphs are, for the player's buttons in their place", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	struct FNoNames final : IMesGameState
	{
		std::span<const uint8> Name(int32) const override { return {}; }
		uint8 PartyName(int32) const override { return 0xFF; }
	} State;
	FMesContext Text{&Resident.Font, &Resident.NameTables, &State, {1, 0, 3, 2, 4, 5, 6, 7}, {}};
	FEventContext Context;
	FFieldMessageWindow& Window = OpenWindow(Context, 0, 0);
	Window.OpenTicks = 0;
	Window.Text.emplace(20, 3);
	// "Hit (Cross) now": the font's narrow letters, then the button's glyph.
	Window.Text->Queue(std::vector<uint8>{0x27, 0x45, 0x50, 0x10, 0x7F, 0x10, 0x4A, 0x4B, 0x53, 0x00});
	for (int32 Tick = 0; Tick < 30; ++Tick)
	{
		Window.Text->Update(Text);
	}
	FDrawList Layer;
	std::vector<FTextButtonCell> Cells;
	BuildFieldScreenLayer(Context, Tables(), EFieldFrame::Tick, Layer, nullptr, nullptr, &Cells);
	REQUIRE(Cells.size() == 1);
	CHECK(Cells[0].Button == EPadButton::Cross);
	const FDrawPrimitive& Line = Layer.Primitives[Cells[0].Primitive];
	CHECK(Line.Texture);
	const auto& Glyphs = Window.Text->Row(0).Glyphs;
	const auto Cross = std::find_if(Glyphs.begin(), Glyphs.end(), [](const FMesPlacedGlyph& Glyph) { return Glyph.Code.Code == 0x7F; });
	REQUIRE(Cross != Glyphs.end());
	CHECK(Cells[0].OffsetX == Cross->XUnits * 4);

	// The character files' map uses the glyphs as bullets: they stay the font's.
	Context.RunningMap = 580;
	BuildFieldScreenLayer(Context, Tables(), EFieldFrame::Tick, Layer, nullptr, nullptr, &Cells);
	CHECK(Cells.empty());
}

TEST_CASE("A message window's line tells where it names a button in words, and where the line ends", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	struct FNoNames final : IMesGameState
	{
		std::span<const uint8> Name(int32) const override { return {}; }
		uint8 PartyName(int32) const override { return 0xFF; }
	} State;
	FMesContext Text{&Resident.Font, &Resident.NameTables, &State, {1, 0, 3, 2, 4, 5, 6, 7}, {}};
	FEventContext Context;
	FFieldMessageWindow& Window = OpenWindow(Context, 0, 0);
	Window.OpenTicks = 0;
	Window.Text.emplace(60, 3);
	std::vector<uint8> Message;
	for (const char Character : std::string_view("Push the R1 button now"))
	{
		Message.push_back(MesCodeOfCharacter(Character)->Code);
	}
	Message.push_back(0x00);
	Window.Text->Queue(Message);
	for (int32 Tick = 0; Tick < 40; ++Tick)
	{
		Window.Text->Update(Text);
	}
	FDrawList Layer;
	std::vector<FTextButtonCell> Cells;
	BuildFieldScreenLayer(Context, Tables(), EFieldFrame::Tick, Layer, nullptr, nullptr, &Cells);
	REQUIRE(Cells.size() == 1);
	CHECK(Cells[0].Button == EPadButton::R1);
	CHECK_FALSE(Cells[0].bDirections);
	const auto& Glyphs = Window.Text->Row(0).Glyphs;
	REQUIRE(Glyphs.size() > 12);
	// "R1" is the ninth and tenth glyph, the space after it the eleventh.
	CHECK(Cells[0].OffsetX == Glyphs[9].XUnits * 4);
	CHECK(Cells[0].Width == (Glyphs[11].XUnits - Glyphs[9].XUnits) * 4);
	CHECK(Cells[0].LineEnd == Cells[0].Primitive + 1);
}

TEST_CASE("A button named in words shows from its first letter while the line is typed", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	struct FNoNames final : IMesGameState
	{
		std::span<const uint8> Name(int32) const override { return {}; }
		uint8 PartyName(int32) const override { return 0xFF; }
	} State;
	FMesContext Text{&Resident.Font, &Resident.NameTables, &State, {1, 0, 3, 2, 4, 5, 6, 7}, {}};
	FEventContext Context;
	Context.Messages.Text.Font = &Resident.Font;
	FFieldMessageWindow& Window = OpenWindow(Context, 0, 0);
	Window.OpenTicks = 0;
	Window.Text.emplace(60, 3);
	Window.Text->SetSpeed(1);
	std::vector<uint8> Message;
	for (const char Character : std::string_view("Push the R1 button now"))
	{
		Message.push_back(MesCodeOfCharacter(Character)->Code);
	}
	Message.push_back(0x00);
	Window.Text->Queue(Message);
	const auto Typed = [&] { return Window.Text->Row(0).Glyphs.size(); };
	FDrawList Layer;
	std::vector<FTextButtonCell> Cells;
	// Typed to "Push the": nothing yet; to "Push the R": the name from its R to the typed end.
	for (int32 Tick = 0; Tick < 200 && Typed() < 8; ++Tick)
	{
		Window.Text->Update(Text);
	}
	BuildFieldScreenLayer(Context, Tables(), EFieldFrame::Tick, Layer, nullptr, nullptr, &Cells);
	CHECK(Cells.empty());
	for (int32 Tick = 0; Tick < 200 && Typed() < 10; ++Tick)
	{
		Window.Text->Update(Text);
	}
	REQUIRE(Typed() == 10);
	BuildFieldScreenLayer(Context, Tables(), EFieldFrame::Tick, Layer, nullptr, nullptr, &Cells);
	REQUIRE(Cells.size() == 1);
	CHECK(Cells[0].Button == EPadButton::R1);
	CHECK(Cells[0].OffsetX == Window.Text->Row(0).Glyphs[9].XUnits * 4);
	CHECK(Cells[0].OffsetX + Cells[0].Width == Window.Text->Row(0).WidthUnits * 4);
}
