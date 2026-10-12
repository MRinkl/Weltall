#include "Graphics/Gte.h"

#include <catch_amalgamated.hpp>

#include <array>

using namespace Weltall;

TEST_CASE("The GTE projects through its division unit and flags what leaves its ranges", "[Graphics]")
{
	FGte Gte;
	Gte.Transform = FGteMatrix::Identity();
	Gte.Transform.T = {0, 0, 1000};
	Gte.ProjectionDistance = 150;
	// 150 / 1000 comes out of the division unit as 9830 / 65536: 983000 >> 16 = 14, 491500 >> 16 = 7.
	const FGteProjection Near = Gte.Rtps({100, 50, 0});
	CHECK(Near.ScreenZ == 1000);
	CHECK(Near.ScreenX == 14);
	CHECK(Near.ScreenY == 7);
	CHECK(Near.Flag == 0);

	// Behind the eye: SZ3 saturates at 0 and the division overflows.
	Gte.Transform.T = {0, 0, -10};
	const FGteProjection Behind = Gte.Rtps({0, 0, 0});
	CHECK(Behind.ScreenZ == 0);
	CHECK((Behind.Flag & GteFlag::DepthSaturated) != 0);
	CHECK((Behind.Flag & GteFlag::DivideOverflow) != 0);
	CHECK((Behind.Flag & GteFlag::Error) != 0);

	// Past a view depth of 7FFFh IR3 saturates although SZ3 still holds the depth; that flag is not
	// one of those that make up the error bit.
	Gte.Transform.T = {0, 0, 40000};
	const FGteProjection Far = Gte.Rtps({0, 0, 0});
	CHECK(Far.ScreenZ == 40000);
	CHECK(Far.Flag == GteFlag::Ir3Saturated);
}

TEST_CASE("NCLIP is positive for triangles that run clockwise on screen", "[Graphics]")
{
	CHECK(Nclip(0, 0, 10, 0, 0, 10) == 100);
	CHECK(Nclip(0, 0, 0, 10, 10, 0) == -100);
}

TEST_CASE("libgte's sines are rounded exactly", "[Graphics]")
{
	CHECK(GteSin(0) == 0);
	CHECK(GteSin(1) == 6);
	CHECK(GteSin(1024) == 4096);
	CHECK(GteCos(2048) == -4096);
	CHECK(GteSin(-1) == -6);
	// A quarter turn about y takes +z to +x.
	const FGteMatrix QuarterTurn = RotationY(1024);
	CHECK(QuarterTurn.M == std::array<std::array<int16, 3>, 3>{{{0, 0, 4096}, {0, 4096, 0}, {-4096, 0, 0}}});
}

TEST_CASE("libgte's matrix routines keep its fixed-point arithmetic", "[Graphics]")
{
	FGteMatrix Scaled = ScaleRows(RotationY(2048), {0x4C6F, 0x4C6F, 0x4C6F});
	CHECK(Scaled.M[0][0] == -19567);
	CHECK(Scaled.M[1][1] == 19567);
	// ApplyMatrixLV rounds each half toward minus infinity: -19567 * -9800 / 4096 = 46815.6, * 5880 = -28089.3.
	CHECK(ApplyMatrixLV(Scaled, {-9800, 0, 5880}) == std::array<int32, 3>{46815, 0, -28090});
	// Components past 15 bits go through the high half.
	FGteMatrix Half{};
	Half.M = {{{2048, 0, 0}, {0, 2048, 0}, {0, 0, 2048}}};
	CHECK(ApplyMatrixLV(Half, {100001, -100001, 32768}) == std::array<int32, 3>{50000, -50001, 16384});
}

TEST_CASE("libgte's MulMatrix saturates each entry and keeps the left matrix's translation", "[Graphics]")
{
	FGteMatrix QuarterTurn = RotationY(1024);
	QuarterTurn.T = {1, 2, 3};
	const FGteMatrix HalfTurn = MulMatrix(QuarterTurn, RotationY(1024));
	CHECK(HalfTurn.M == std::array<std::array<int16, 3>, 3>{{{-4096, 0, 0}, {0, 4096, 0}, {0, 0, -4096}}});
	CHECK(HalfTurn.T == std::array<int32, 3>{1, 2, 3});

	// 4.0 * 4.0 = 16.0 does not fit the 16-bit entry: it saturates at 7FFFh.
	FGteMatrix Four{};
	Four.M = {{{16384, 0, 0}, {0, 16384, 0}, {0, 0, -16384}}};
	const FGteMatrix Sixteen = MulMatrix(Four, Four);
	CHECK(Sixteen.M[0][0] == 0x7FFF);
	CHECK(Sixteen.M[2][2] == 0x7FFF);
	CHECK(Transpose(QuarterTurn).M == RotationY(-1024).M);
}

TEST_CASE("libgte's ScaleMatrix scales columns and wraps an entry that overflows", "[Graphics]")
{
	FGteMatrix Matrix{};
	Matrix.M = {{{0x5000, 0x1000, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}};
	const FGteMatrix Scaled = ScaleColumns(Matrix, {0x2000, 0x800, 0x1000});
	// 5.0 * 2.0 = 10.0 is 40960, which wraps to -24576 in 16 bits.
	CHECK(Scaled.M[0][0] == -24576);
	CHECK(Scaled.M[0][1] == 0x800);
	CHECK(Scaled.M[1][1] == 0x800);
	CHECK(Scaled.M[2][2] == 0x1000);
}

TEST_CASE("DPCS fades a color toward the far color by the depth cue", "[Graphics]")
{
	FGte Gte;
	Gte.FarColor = {255 * 16, 0, 0};
	// Halfway: 100 + (255 - 100) / 2 truncates to 177; the others halve toward 0.
	CHECK(Gte.DepthCueColor({100, 50, 200, 0x2C}, 0x800) == FGteColor{177, 25, 100, 0x2C});
	CHECK(Gte.DepthCueColor({100, 50, 200, 0x2C}, 0) == FGteColor{100, 50, 200, 0x2C});
	CHECK(Gte.DepthCueColor({100, 50, 200, 0x2C}, 0x1000) == FGteColor{255, 0, 0, 0x2C});
}

TEST_CASE("SetFogNearFar sets the depth cue's slope and base as libgte computes them", "[Graphics]")
{
	FGte Gte;
	// The field's defaults at H 512: -(5600 * 12300 / 6700) << 8 / 512 and (12300 << 12) / 6700 << 12.
	REQUIRE(Gte.SetFogNearFar(0x15E0, 0x300C, 512));
	CHECK(Gte.DepthCueA == -5140);
	CHECK(Gte.DepthCueB == 7519 << 12);
	// A span of 99 or less changes nothing; an H of 0 is the library's division by zero.
	CHECK(Gte.SetFogNearFar(100, 199, 512));
	CHECK(Gte.DepthCueA == -5140);
	CHECK_FALSE(Gte.SetFogNearFar(100, 1000, 0));
}
