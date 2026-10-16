#include "Graphics/TextureSheet.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	FImageRgba8 Solid(int32 Width, int32 Height, uint8 Value)
	{
		return {Width, Height, std::vector<uint8>(static_cast<std::size_t>(Width * Height * 4), Value)};
	}
}

TEST_CASE("A texture sheet lays its pictures out in rows a texel apart and reads back its layout", "[Graphics][TextureSheet]")
{
	const FImageRgba8 Wide = Solid(230, 10, 1);
	const FImageRgba8 Tall = Solid(40, 30, 2);
	const FImageRgba8 Small = Solid(8, 8, 3);
	const FTextureSheetPicture Pictures[] = {{0x10, &Wide}, {0x20, &Tall}, {0x10, &Wide}, {0x30, &Small}};
	const FTextureSheetLayout Layout = LayOutTextureSheet(Pictures);
	REQUIRE(Layout.Cells.size() == 3);
	CHECK(Layout.Cells[0] == FTextureSheetCell{0x10, 0, 0, 230, 10});
	CHECK(Layout.Cells[1] == FTextureSheetCell{0x20, 0, 11, 40, 30});
	CHECK(Layout.Cells[2] == FTextureSheetCell{0x30, 41, 11, 8, 8});
	CHECK(Layout.Width == 230);
	CHECK(Layout.Height == 41);
	const FImageRgba8 Sheet = DrawTextureSheet(Layout, Pictures);
	CHECK(Sheet.Pixels[(11 * 230 + 41) * 4] == 3);
	CHECK(Sheet.Pixels[(11 * 230 + 39) * 4] == 2);
	CHECK(Sheet.Pixels[(11 * 230 + 40) * 4] == 0);
	CHECK(Sheet.Pixels[(10 * 230) * 4] == 0);

	const auto Read = ReadTextureSheetLayout(WriteTextureSheetLayout(Layout));
	REQUIRE(Read);
	CHECK(Read->Width == 230);
	CHECK(Read->Height == 41);
	CHECK(Read->Cells == Layout.Cells);
}

TEST_CASE("A texture sheet's layout refuses pictures off the sheet and keeps the first of a repeated key", "[Graphics][TextureSheet]")
{
	CHECK_FALSE(ReadTextureSheetLayout("0000000000000010\t0\t0\t8\t8\n"));
	CHECK_FALSE(ReadTextureSheetLayout("sheet\t16\t16\n0000000000000010\t10\t0\t8\t8\n"));
	CHECK_FALSE(ReadTextureSheetLayout("sheet\t16\t16\nnot-a-key\t0\t0\t8\t8\n"));
	const auto Read = ReadTextureSheetLayout("# notes\r\nsheet\t16\t16\r\n0000000000000010\t0\t0\t8\t8\r\n0000000000000010\t8\t0\t8\t8\r\n");
	REQUIRE(Read);
	REQUIRE(Read->Cells.size() == 1);
	CHECK(Read->Cells[0].X == 0);
}
