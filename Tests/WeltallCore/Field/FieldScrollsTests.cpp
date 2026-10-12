#include "Field/EventMachine.h"
#include "Game/PsxRandom.h"
#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

#include <memory>

using namespace Weltall;
using Weltall::Testing::FByteWriter;

namespace
{
	/** A map of script actors that each run their script 1 from the start of one bytecode, on models 0, 1... */
	class FScrollMap
	{
	public:
		FScrollMap(int32 Actors, std::vector<uint8> Code)
		{
			FByteWriter Writer;
			for (int32 Word = 0; Word < 32; ++Word)
			{
				Writer.U32(0);
			}
			Writer.U32(static_cast<uint32>(Actors));
			for (int32 Actor = 0; Actor < Actors; ++Actor)
			{
				for (int32 Script = 0; Script < 32; ++Script)
				{
					Writer.U16(Script == 1 ? static_cast<uint16>(Actor * 0x40) : 0);
				}
			}
			Writer.Bytes.insert(Writer.Bytes.end(), Code.begin(), Code.end());
			auto Parsed = FEventScripts::Parse(Writer.Bytes);
			REQUIRE(Parsed);
			Scripts = std::make_unique<FEventScripts>(std::move(*Parsed));
			Context.Scripts = Scripts.get();
			Context.Random = &Random;
			Context.MapModels = Models;
			for (int32 Actor = 0; Actor < Actors; ++Actor)
			{
				FFieldActor Record;
				Record.Flags = 0x200;
				Record.Model = static_cast<uint16>(Actor);
				Context.Actors.push_back(Record);
				Context.States.emplace_back().Reset(Random.Next());
			}
			Context.bInitDone = true;
		}

		std::optional<FError> Tick() { return FEventMachine::Update(Context); }

		std::unique_ptr<FEventScripts> Scripts;
		std::vector<FMdlModel> Models;
		FPsxRandom Random;
		FEventContext Context;
	};

	/** Each actor's script at 0x40 times its number; a script that has run ends in a yield loop. */
	std::vector<uint8> Scripts(std::initializer_list<std::vector<uint8>> PerActor)
	{
		std::vector<uint8> Code;
		for (const std::vector<uint8>& Script : PerActor)
		{
			const uint16 Start = static_cast<uint16>(Code.size());
			Code.insert(Code.end(), Script.begin(), Script.end());
			const uint16 Loop = static_cast<uint16>(Code.size());
			Code.insert(Code.end(), {0xC3, 0x01, static_cast<uint8>(Loop), static_cast<uint8>(Loop >> 8)});
			Code.resize(Start + 0x40u, 0);
		}
		return Code;
	}

	FMdlModel ModelOf(std::initializer_list<EMdlPrimitiveType> Types)
	{
		FMdlModel Model;
		FMdlPart& Part = Model.Parts.emplace_back();
		for (const EMdlPrimitiveType Type : Types)
		{
			FMdlPrimitive& Primitive = Part.Primitives.emplace_back();
			Primitive.Type = Type;
		}
		return Model;
	}

	/** DA: a rectangle at (X, Y), Width x Height in Bands bands, copied to (ToX, ToY), every band at Speed. */
	std::vector<uint8> MakeScroll(uint16 X, uint16 Y, uint16 Width, uint16 Height, uint16 Bands, uint16 ToX, uint16 ToY, uint16 Speed)
	{
		FByteWriter Writer;
		Writer.U8(0xDA);
		for (const uint16 Operand : {X, Y, Width, Height, Bands, ToX, ToY, Speed})
		{
			Writer.U16(Operand);
		}
		return Writer.Bytes;
	}

	std::vector<uint8> Join(std::initializer_list<std::vector<uint8>> Parts)
	{
		std::vector<uint8> Bytes;
		for (const std::vector<uint8>& Part : Parts)
		{
			Bytes.insert(Bytes.end(), Part.begin(), Part.end());
		}
		return Bytes;
	}

	bool SameMove(const FFieldVramOperation& Move, FVramRect Rect, int16 ToX, int16 ToY)
	{
		return Move.Rect.X == Rect.X && Move.Rect.Y == Rect.Y && Move.Rect.Width == Rect.Width && Move.Rect.Height == Rect.Height && Move.ToX == ToX
			&& Move.ToY == ToY && Move.Kind == EFieldVramChange::Copy;
	}
}

TEST_CASE("A VRAM scroller copies each band rotated left by its offset's whole texels every tick", "[Field]")
{
	// Two bands of 4 rows, 16 texels wide: 1.5 texels a tick, then band 1 set to 2 texels a tick back.
	FScrollMap Map(1, Scripts({Join({MakeScroll(100, 10, 16, 9, 2, 200, 20, 0x0118), {0xFE, 0x40, 0x00, 0x80, 0x01, 0x80, 0xE0, 0x00}})}));
	REQUIRE_FALSE(Map.Tick());
	FFieldRenderState& Render = Map.Context.Render;
	REQUIRE(Render.LineScrolls.size() == 1);
	CHECK(Render.LineScrolls[0].BandHeight == 4);
	CHECK(Render.LineScrolls[0].Speeds == std::vector<int8>{0x18, -0x20});

	Render.TickLineScrolls(false);
	const std::vector<FFieldVramOperation>& Moves = Render.LineScrollMoves;
	REQUIRE(Moves.size() == 4);
	CHECK(SameMove(Moves[0], {100, 10, 1, 4}, 215, 20));
	CHECK(SameMove(Moves[1], {101, 10, 15, 4}, 200, 20));
	// -2 texels: the 16ths keep their sign into 16 bits, FFFEh, which is 14 modulo 16.
	CHECK(SameMove(Moves[2], {100, 14, 14, 4}, 202, 24));
	CHECK(SameMove(Moves[3], {114, 14, 2, 4}, 200, 24));

	// Band 1 comes round to no shift: its first copy has no width, and only the whole band is copied.
	for (int32 Tick = 0; Tick < 7; ++Tick)
	{
		Render.TickLineScrolls(false);
	}
	REQUIRE(Moves.size() == 3);
	CHECK(SameMove(Moves[1], {112, 10, 4, 4}, 200, 20));
	CHECK(SameMove(Moves[2], {100, 14, 16, 4}, 200, 24));
}

TEST_CASE("A scroller's offsets wrap in 16ths through 16 bits, whatever its width", "[Field]")
{
	FFieldLineScroll Scroll = FFieldLineScroll::Make(0, 0, 30, 1, 1, 0, 0, -16);
	Scroll.Offsets[0] = -16;
	CHECK(Scroll.BandShift(0) == 0xFFFF % 30);
	Scroll.Offsets[0] = 0x7FF0;
	CHECK(Scroll.BandShift(0) == 0x7FF % 30);
}

TEST_CASE("With the fix a scroller's band turns past its start and past 16 bits without a jump", "[Field]")
{
	// Map 147's width, 30 texels, a texel a tick back from the start, then a texel a tick on past 7FFh texels.
	FFieldRenderState Render;
	Render.LineScrolls.push_back(FFieldLineScroll::Make(0, 0, 30, 1, 1, 0, 0, -16));
	Render.TickLineScrolls(true);
	CHECK(Render.LineScrolls[0].BandShift(0) == 29);
	Render.TickLineScrolls(true);
	CHECK(Render.LineScrolls[0].BandShift(0) == 28);

	Render.LineScrolls[0].Speeds[0] = 16;
	uint16 Shift = Render.LineScrolls[0].BandShift(0);
	for (int32 Tick = 0; Tick < 0x900; ++Tick)
	{
		Render.TickLineScrolls(true);
		const uint16 Next = Render.LineScrolls[0].BandShift(0);
		REQUIRE(Next == (Shift + 1) % 30);
		Shift = Next;
	}

	// Without it, the band jumps by 65536 modulo 30 texels as it runs back past its start.
	FFieldRenderState Original;
	Original.LineScrolls.push_back(FFieldLineScroll::Make(0, 0, 30, 1, 1, 0, 0, -16));
	Original.TickLineScrolls(false);
	CHECK(Original.LineScrolls[0].BandShift(0) == 0xFFFF % 30);
}

TEST_CASE("A band's speed goes only to a band the scroller has; a negative band stops the scripts", "[Field]")
{
	// Band 3 of a 2-band scroller, then scroller 5, which the map has not made: nothing changes.
	FScrollMap Map(1, Scripts({Join({MakeScroll(0, 0, 8, 8, 2, 8, 0, 1), {0xFE, 0x40, 0x00, 0x80, 0x03, 0x80, 0x07, 0x00},
		{0xFE, 0x40, 0x05, 0x80, 0x00, 0x80, 0x07, 0x00}, {0xFE, 0x40, 0x00, 0x80, 0x00, 0x04, 0x07, 0x00}})}));
	Map.Context.Variables.Set(0x400, -1);
	const auto Error = Map.Tick();
	REQUIRE(Error);
	CHECK(Error->Message.find("index -1") != std::string::npos);
	CHECK(Map.Context.Render.LineScrolls[0].Speeds == std::vector<int8>{1, 1});
}

TEST_CASE("A scroller without bands or width stops the scripts, and the map makes 32 at most", "[Field]")
{
	FScrollMap NoBands(1, Scripts({MakeScroll(0, 0, 8, 8, 0, 8, 0, 1)}));
	REQUIRE(NoBands.Tick());
	FScrollMap NoWidth(1, Scripts({MakeScroll(0, 0, 0, 8, 1, 8, 0, 1)}));
	REQUIRE(NoWidth.Tick());

	FScrollMap Full(1, Scripts({MakeScroll(0, 0, 8, 8, 1, 8, 0, 1)}));
	Full.Context.Render.LineScrolls.resize(FFieldRenderState::MaxLineScrolls, FFieldLineScroll::Make(0, 0, 1, 1, 1, 0, 0, 0));
	REQUIRE_FALSE(Full.Tick());
	CHECK(Full.Context.Render.LineScrolls.size() == FFieldRenderState::MaxLineScrolls);
}

TEST_CASE("Texture scrolls add to a model's coordinates modulo 256 and refuse a model with other polygons", "[Field]")
{
	FScrollMap Map(2, Scripts({{0xFE, 0x1B, 0xF0, 0xFF, 0x10, 0x00, 0xFE, 0x1B, 0x20, 0x00, 0xF8, 0x01}, {0xFE, 0x1B, 0x01, 0x00, 0x01, 0x00}}));
	Map.Models = {ModelOf({EMdlPrimitiveType::StaticTexturedQuad, EMdlPrimitiveType::TexturedTriangle}), ModelOf({EMdlPrimitiveType::StaticGouraudTexturedTriangle})};
	Map.Context.MapModels = Map.Models;
	const auto Error = Map.Tick();
	REQUIRE(Error);
	CHECK(Error->Message.find("actor 1 has no model of flat-textured polygons") != std::string::npos);
	CHECK(Map.Context.Render.ModelUvScroll(0) == FMdlUv{0x10, 0x08});
	CHECK(Map.Context.Render.ModelUvScroll(1) == FMdlUv{});
}

TEST_CASE("The screen wave eases its parameters, steps its phases and fades out to off, drawn that tick", "[Field]")
{
	FFieldScreenWave Wave;
	Wave.bOn = true;
	Wave.MoveTo({4, 2, 0x100, 0x80, 0x40, -0x20}, 2);
	CHECK(Wave.Steps == std::array<int32, 6>{0x20000, 0x10000, 0x800000, 0x400000, 0x200000, -0x100000});
	Wave.Tick();
	CHECK(Wave.Ticks == 1);
	CHECK(Wave.WholePart(FFieldScreenWave::AmplitudeX) == 2);
	CHECK((Wave.PhaseX == 0x20 && Wave.PhaseY == -0x10));
	Wave.Tick();
	Wave.Tick();
	CHECK(Wave.Ticks == 0);
	CHECK((Wave.PhaseX == 0xA0 && Wave.PhaseY == -0x50));
	CHECK(Wave.bDrawn);

	// A fade to nothing over no ticks takes one; the next tick switches the wave off and still draws it.
	Wave.MoveTo({}, 0);
	Wave.bOffWhenDone = true;
	Wave.Tick();
	CHECK((Wave.bOn && Wave.Parameters == std::array<int32, 6>{}));
	Wave.Parameters[FFieldScreenWave::AmplitudeX] = 0x30000;
	Wave.Tick();
	CHECK((!Wave.bOn && Wave.bDrawn && Wave.Parameters == std::array<int32, 6>{}));
	Wave.Tick();
	CHECK_FALSE(Wave.bDrawn);
}

TEST_CASE("FE 27 waits while the screen wave is on, and its fade out ends with the wave off", "[Field]")
{
	// FE 26 (amplitude 4 over 2 ticks), FE 27 fade out over 1 tick, FE 27 wait, then count.
	FScrollMap Map(1, Scripts({{0xFE, 0x26, 0x04, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x02, 0x80,
		0xFE, 0x27, 0x00, 0x01, 0x80, 0xFE, 0x27, 0x01, 0x3C, 0x00, 0x04}}));
	FFieldScreenWave& Wave = Map.Context.Render.ScreenWave;
	for (int32 Tick = 0; Tick < 3; ++Tick)
	{
		REQUIRE_FALSE(Map.Tick());
		Wave.Tick();
	}
	CHECK(Map.Context.Variables.Get(0x400, *Map.Scripts) == 0);
	CHECK((Wave.bOffWhenDone && !Wave.bOn));
	REQUIRE_FALSE(Map.Tick());
	CHECK(Map.Context.Variables.Get(0x400, *Map.Scripts) == 1);
}

TEST_CASE("The screen wave's cells sway by column and row, the last rows' right edge with column 19", "[Field]")
{
	FFieldScreenWave Wave;
	std::array<FFieldScreenWaveCell, FFieldScreenWave::Columns * FFieldScreenWave::Rows> Cells;
	Wave.BuildCells(Cells);
	for (int32 Row = 0; Row < FFieldScreenWave::Rows; ++Row)
	{
		for (int32 Column = 0; Column < FFieldScreenWave::Columns; ++Column)
		{
			const auto& Corners = Cells[static_cast<std::size_t>(Row * FFieldScreenWave::Columns + Column)].Corners;
			CHECK((Corners[0] == std::array<int32, 2>{16 * Column, 16 * Row} && Corners[3] == std::array<int32, 2>{16 * Column + 16, 16 * Row + 16}));
		}
	}

	// Amplitude 8 across with the columns half a turn apart from a phase of 0: column c sways by 8 cos(c/2 turn).
	Wave.Parameters[FFieldScreenWave::AmplitudeX] = 8 << 16;
	Wave.Parameters[FFieldScreenWave::ColumnStep] = 0x800 << 16;
	Wave.Parameters[FFieldScreenWave::AmplitudeY] = 3 << 16;
	Wave.BuildCells(Cells);
	CHECK(Wave.ColumnOffset(1) == -8);
	CHECK(Wave.ColumnOffset(19) == -8);
	CHECK(Wave.RowOffset(5) == 3);
	const auto& Top = Cells[FFieldScreenWave::Columns - 1].Corners;
	CHECK((Top[0] == std::array<int32, 2>{16 * 19 - 8, 0} && Top[3] == std::array<int32, 2>{320, 16 + 3}));
	const auto& Bottom = Cells.back().Corners;
	CHECK((Bottom[0] == std::array<int32, 2>{16 * 19 - 8, 16 * 13 + 3} && Bottom[3] == std::array<int32, 2>{320 - 8, 224}));
}

TEST_CASE("A wide view's wave carries the cosines across it, the screen's own side edges swaying too", "[Field]")
{
	constexpr int32 Wide = FFieldScreenWave::WideColumns;
	constexpr int32 Margin = FFieldScreenWave::WideMargin;
	FFieldScreenWave Wave;
	std::vector<FFieldScreenWaveCell> Cells(static_cast<std::size_t>(Wide * FFieldScreenWave::Rows));
	const std::span<FFieldScreenWaveCell, Wide * FFieldScreenWave::Rows> All(Cells);
	// At rest every cell is its own square, from past the widest view's left edge to past its right.
	Wave.BuildWideCells(All);
	CHECK(Cells.front().Corners[0] == std::array<int32, 2>{-16 * Margin, 0});
	CHECK(Cells.back().Corners[3] == std::array<int32, 2>{320 + 16 * Margin, 224});
	CHECK(16 * Margin >= 160 * 32 * 3 / (9 * 4) - 160 + 16);

	// A quarter turn a column from a phase of 0: column c sways by 8 cos(c/4 turn), on either side of the screen as on it.
	Wave.Parameters[FFieldScreenWave::AmplitudeX] = 8 << 16;
	Wave.Parameters[FFieldScreenWave::ColumnStep] = 0x400 << 16;
	Wave.Parameters[FFieldScreenWave::AmplitudeY] = 3 << 16;
	Wave.BuildWideCells(All);
	std::array<FFieldScreenWaveCell, FFieldScreenWave::Columns * FFieldScreenWave::Rows> Screen;
	Wave.BuildCells(Screen);
	const auto Cell = [&Cells](int32 Row, int32 Column) -> const auto& { return Cells[static_cast<std::size_t>(Row * Wide + Column + Margin)].Corners; };
	CHECK(Cell(0, -4)[0] == std::array<int32, 2>{-64 + 8, 0});
	CHECK(Cell(0, -2)[0] == std::array<int32, 2>{-32 - 8, 0});
	CHECK(Cell(0, 0)[0] == std::array<int32, 2>{8, 0});
	CHECK(Cell(13, 19)[3] == std::array<int32, 2>{320 + 8, 224});
	CHECK(Cell(13, 24)[0] == std::array<int32, 2>{16 * 24 + 8, 16 * 13 + 3});
	// Between the screen's side edges the cells are the screen's own, but for its last rows' right edge.
	for (int32 Row = 0; Row < FFieldScreenWave::Rows; ++Row)
	{
		for (int32 Column = 1; Column + 1 < FFieldScreenWave::Columns; ++Column)
		{
			CHECK(Cell(Row, Column) == Screen[static_cast<std::size_t>(Row * FFieldScreenWave::Columns + Column)].Corners);
		}
	}
	// Each cell begins where the one before it ends: no gap anywhere across the view.
	for (int32 Column = -Margin; Column + 1 < FFieldScreenWave::Columns + Margin; ++Column)
	{
		CHECK(Cell(5, Column)[1] == Cell(5, Column + 1)[0]);
	}
}
