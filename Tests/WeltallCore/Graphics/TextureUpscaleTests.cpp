#include "Graphics/TextureUpscale.h"
#include "Support/XorShift32.h"

#include <catch_amalgamated.hpp>

#include <string>

using namespace Weltall;

namespace
{
	/** An index image from rows of digits, padded with zeros to Size x Size. */
	FTextureIndices Drawn(std::initializer_list<const char*> Rows, int32 Size = 16)
	{
		FTextureIndices Image{Size, Size, std::vector<uint8>(static_cast<std::size_t>(Size * Size))};
		int32 Y = 0;
		for (const char* Row : Rows)
		{
			for (int32 X = 0; Row[X] != '\0'; ++X)
			{
				Image.Indices[static_cast<std::size_t>(Y * Size + X)] = static_cast<uint8>(Row[X] - '0');
			}
			++Y;
		}
		return Image;
	}

	/** Rows of the doubled image from (X, Y), Width x Height of them, as digits. */
	std::vector<std::string> RowsOf(const FTextureIndices& Image, int32 X, int32 Y, int32 Width, int32 Height)
	{
		std::vector<std::string> Rows;
		for (int32 Row = Y; Row < Y + Height; ++Row)
		{
			std::string Text;
			for (int32 Column = X; Column < X + Width; ++Column)
			{
				Text += static_cast<char>('0' + Image.At(Column, Row));
			}
			Rows.push_back(Text);
		}
		return Rows;
	}

	FVramImage RandomImage(Testing::FXorShift32& Random, int16 X, int16 Y, int16 Width, int16 Height, uint32 Kinds)
	{
		FVramImage Image{{X, Y, Width, Height}, std::vector<uint16>(static_cast<std::size_t>(Width * Height))};
		for (uint16& Unit : Image.Units)
		{
			// Few kinds of nibble, so that neighbours often agree and the staircase rule has steps to find.
			uint16 Value = 0;
			for (int32 Nibble = 0; Nibble < 4; ++Nibble)
			{
				Value = static_cast<uint16>(Value | (Random.Below(Kinds)) << (Nibble * 4));
			}
			Unit = Value;
		}
		return Image;
	}

	bool SameIndices(const FTextureIndices& A, const FTextureIndices& B)
	{
		return A.Width == B.Width && A.Height == B.Height && A.Indices == B.Indices;
	}
}

TEST_CASE("A one-texel staircase doubles into a smooth diagonal, the higher index taking the corners", "[Graphics]")
{
	const FTextureIndices Rising = TextureUpscale::Double(Drawn({
		"1112",
		"1122",
		"1222",
		"2222",
		"2222",
	}));
	CHECK(RowsOf(Rising, 0, 0, 8, 8) == std::vector<std::string>{
		"11111122",
		"11111222",
		"11112222",
		"11122222",
		"11222222",
		"12222222",
		"22222222",
		"22222222",
	});
	const FTextureIndices Falling = TextureUpscale::Double(Drawn({
		"2221",
		"2211",
		"2111",
		"1111",
		"1111",
	}));
	CHECK(RowsOf(Falling, 0, 0, 8, 8) == std::vector<std::string>{
		"22222211",
		"22222211",
		"22222111",
		"22221111",
		"22211111",
		"22111111",
		"11111111",
		"11111111",
	});
}

TEST_CASE("Doubling keeps a block's corners, a single texel and a checkerboard", "[Graphics]")
{
	const FTextureIndices Shapes = TextureUpscale::Double(Drawn({
		"00000000",
		"03300400",
		"03300000",
	}));
	CHECK(RowsOf(Shapes, 0, 0, 12, 6) == std::vector<std::string>{
		"000000000000",
		"000000000000",
		"003333000044",
		"003333000044",
		"003333000000",
		"003333000000",
	});
	FTextureIndices Checkerboard{16, 16, std::vector<uint8>(16 * 16)};
	for (int32 Y = 0; Y < 16; ++Y)
	{
		for (int32 X = 0; X < 16; ++X)
		{
			Checkerboard.Indices[static_cast<std::size_t>(Y * 16 + X)] = static_cast<uint8>((X + Y) % 2 == 0 ? 6 : 7);
		}
	}
	const FTextureIndices Doubled = TextureUpscale::Double(Checkerboard);
	int32 Unlike = 0;
	for (int32 Y = 0; Y < 32; ++Y)
	{
		for (int32 X = 0; X < 32; ++X)
		{
			Unlike += Doubled.At(X, Y) == Checkerboard.At(X / 2, Y / 2) ? 0 : 1;
		}
	}
	CHECK(Unlike == 0);
}

TEST_CASE("A line one texel wide doubles into a joined band, whichever index is higher", "[Graphics]")
{
	const FTextureIndices Line = TextureUpscale::Double(Drawn({
		"7777777",
		"7177777",
		"7717777",
		"7771777",
		"7777177",
		"7777777",
	}));
	CHECK(RowsOf(Line, 0, 0, 14, 12) == std::vector<std::string>{
		"77777777777777",
		"77777777777777",
		"77117777777777",
		"77111777777777",
		"77711177777777",
		"77771117777777",
		"77777111777777",
		"77777711177777",
		"77777771117777",
		"77777777117777",
		"77777777777777",
		"77777777777777",
	});
}

TEST_CASE("Where two lines cross at a corner, the higher index takes it and the other line is cut", "[Graphics]")
{
	const FTextureIndices Crossing = TextureUpscale::Double(Drawn({
		"11111",
		"12311",
		"13211",
		"11111",
	}));
	CHECK(RowsOf(Crossing, 0, 0, 10, 8) == std::vector<std::string>{
		"1111111111",
		"1111111111",
		"1122331111",
		"1123331111",
		"1133321111",
		"1133221111",
		"1111111111",
		"1111111111",
	});
}

TEST_CASE("Every doubled texel takes an index of its texel or a neighbour's", "[Graphics]")
{
	Testing::FXorShift32 Random(61);
	FTextureIndices Image{32, 32, std::vector<uint8>(32 * 32)};
	for (uint8& Index : Image.Indices)
	{
		Index = static_cast<uint8>(Random.Below(3));
	}
	const FTextureIndices Doubled = TextureUpscale::Double(Image);
	int32 Changed = 0;
	for (int32 Y = 0; Y < Doubled.Height; ++Y)
	{
		for (int32 X = 0; X < Doubled.Width; ++X)
		{
			const uint8 Index = Doubled.At(X, Y);
			const bool bOwn = Index == Image.At(X / 2, Y / 2);
			Changed += bOwn ? 0 : 1;
			const bool bBeside = Index == Image.At(X / 2 + ((X & 1) != 0 ? 1 : -1), Y / 2);
			CHECK((bOwn || bBeside));
		}
	}
	CHECK(Changed > 0);
}

TEST_CASE("A separate texture upscales the same whatever lies around it", "[Graphics]")
{
	Testing::FXorShift32 Random(12);
	FVram Vram;
	FVramImage Cell = RandomImage(Random, 8, 8, 3, 5, 2);
	Cell.bSeparateTexture = true;
	Vram.Upload(Cell);
	CHECK(Vram.EdgesAt(8, 8) == (VramEdge::Left | VramEdge::Top));
	CHECK(Vram.EdgesAt(9, 10) == 0);
	CHECK(Vram.EdgesAt(10, 12) == (VramEdge::Right | VramEdge::Bottom));
	const FTextureIndices Alone = TextureUpscale::Upscale(Vram, ETextureDepth::Indexed4);
	// Texels next to the cell on every side, of the kinds the cell's texels are, then the cell again.
	Vram.Upload(RandomImage(Random, 6, 6, 7, 2, 2));
	Vram.Upload(RandomImage(Random, 6, 13, 7, 2, 2));
	Vram.Upload(RandomImage(Random, 6, 8, 2, 5, 2));
	Vram.Upload(RandomImage(Random, 11, 8, 2, 5, 2));
	Vram.Upload(Cell);
	const FTextureIndices Surrounded = TextureUpscale::Upscale(Vram, ETextureDepth::Indexed4);
	constexpr int32 Scale = TextureUpscale::Factor;
	int32 Unlike = 0;
	for (int32 Y = 8 * Scale; Y < 13 * Scale; ++Y)
	{
		for (int32 X = 8 * 4 * Scale; X < 11 * 4 * Scale; ++X)
		{
			Unlike += Alone.At(X, Y) == Surrounded.At(X, Y) ? 0 : 1;
		}
	}
	CHECK(Unlike == 0);
	// Without its edges, the same cell's border reads what lies beside it.
	Cell.bSeparateTexture = false;
	Vram.Upload(Cell);
	const FTextureIndices Open = TextureUpscale::Upscale(Vram, ETextureDepth::Indexed4);
	int32 Read = 0;
	for (int32 Y = 8 * Scale; Y < 13 * Scale; ++Y)
	{
		for (int32 X = 8 * 4 * Scale; X < 11 * 4 * Scale; ++X)
		{
			Read += Open.At(X, Y) == Surrounded.At(X, Y) ? 0 : 1;
		}
	}
	CHECK(Read > 0);
}

TEST_CASE("VRAM is read as 4-bit and 8-bit indices, a unit's lowest bits first", "[Graphics]")
{
	FVram Vram;
	Vram.Upload({{1023, 511, 2, 1}, {0x4321, 0xBA98}});
	const FTextureIndices Four = TextureUpscale::ReadIndices(Vram, ETextureDepth::Indexed4);
	CHECK(Four.Width == 4096);
	CHECK(Four.Height == 512);
	CHECK(RowsOf(Four, 4092, 511, 4, 1) == std::vector<std::string>{"1234"});
	CHECK(Four.At(4096, 511) == 8);
	CHECK(Four.At(4099, 511) == 0xB);
	const FTextureIndices Eight = TextureUpscale::ReadIndices(Vram, ETextureDepth::Indexed8);
	CHECK(Eight.Width == 2048);
	CHECK(Eight.At(2046, 511) == 0x21);
	CHECK(Eight.At(2047, 511) == 0x43);
	CHECK(Eight.At(-1, -1) == 0x43);
	CHECK(Eight.At(0, 511) == 0x98);
	CHECK(TextureUpscale::Upscale(Vram, ETextureDepth::Indexed8).Width == 2048 * TextureUpscale::Factor);
}

TEST_CASE("VRAM knows which of its tiles an upload wrote since a revision", "[Graphics]")
{
	FVram Vram;
	CHECK(Vram.TilesChangedSince(0) == FVramTiles{});
	Vram.Upload({{60, 14, 8, 4}, std::vector<uint16>(32, 1)});
	const uint32 First = Vram.Revision();
	// Across a corner of four tiles.
	FVramTiles Expected{};
	Expected[1] = 0b11000;
	Expected[2] = 0b11000;
	CHECK(Vram.TilesChangedSince(0) == Expected);
	CHECK(Vram.TilesChangedSince(First) == FVramTiles{});
	// Across VRAM's corner, wrapping.
	Vram.Upload({{1020, 510, 8, 4}, std::vector<uint16>(32, 2)});
	FVramTiles Wrapped{};
	Wrapped[0] = uint64{1} | uint64{1} << 63;
	Wrapped[63] = uint64{1} | uint64{1} << 63;
	CHECK(Vram.TilesChangedSince(First) == Wrapped);
	// An empty upload writes nothing.
	const uint32 Second = Vram.Revision();
	Vram.Upload({{0, 0, 0, 5}, {}});
	CHECK(Vram.TilesChangedSince(Second) == FVramTiles{});
}

TEST_CASE("The rectangles to redo cover the changed tiles and their neighbours, within VRAM", "[Graphics]")
{
	FVramTiles One{};
	One[5] = 1u << 7;
	CHECK(TextureUpscale::RectsToRedo(One) == std::vector<FVramRect>{{6 * 16, 4 * 8, 3 * 16, 3 * 8}});
	// At VRAM's corner the neighbours wrap: four pieces, none across an edge.
	FVramTiles Corner{};
	Corner[0] = 1;
	const std::vector<FVramRect> Pieces = TextureUpscale::RectsToRedo(Corner);
	CHECK(Pieces == std::vector<FVramRect>{{0, 0, 32, 16}, {1008, 0, 16, 16}, {0, 504, 32, 8}, {1008, 504, 16, 8}});
	FVramTiles All{};
	All = FVram::EveryTile();
	CHECK(TextureUpscale::RectsToRedo(All) == std::vector<FVramRect>{{0, 0, 1024, 512}});
	CHECK(TextureUpscale::RectsToRedo(FVramTiles{}).empty());
}

TEST_CASE("Upscaling again over the tiles an upload changed gives what upscaling everything gives", "[Graphics]")
{
	for (const ETextureDepth Depth : {ETextureDepth::Indexed4, ETextureDepth::Indexed8})
	{
		Testing::FXorShift32 Random(Depth == ETextureDepth::Indexed4 ? 4 : 8);
		FVram Vram;
		Vram.Upload(RandomImage(Random, 0, 0, 1024, 512, 3));
		FTextureIndices Doubled = TextureUpscale::Double(TextureUpscale::ReadIndices(Vram, Depth));
		FTextureIndices Upscaled = TextureUpscale::Double(Doubled);
		const uint32 Before = Vram.Revision();
		// Inside a tile, across tiles, and across VRAM's corner.
		Vram.Upload(RandomImage(Random, 70, 20, 5, 3, 2));
		FVramImage Cell = RandomImage(Random, 500, 200, 90, 40, 3);
		Cell.bSeparateTexture = true;
		Vram.Upload(Cell);
		Vram.Upload(RandomImage(Random, 1022, 510, 4, 4, 2));
		const std::vector<FVramRect> Rects = TextureUpscale::RectsToRedo(Vram.TilesChangedSince(Before));
		TextureUpscale::Redo(Vram, Depth, Rects, Doubled, Upscaled);
		const bool bSame = SameIndices(Upscaled, TextureUpscale::Upscale(Vram, Depth));
		CHECK(bSame);
	}
}
