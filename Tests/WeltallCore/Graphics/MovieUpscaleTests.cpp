#include "Graphics/MovieCleanup.h"
#include "Graphics/MovieUpscale.h"
#include "Support/XorShift32.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace Weltall;

namespace
{
	constexpr int32 LevelSize = 1 << MdecSampleFractionBits;

	FMdecPicture FlatPicture(int32 Width, int32 Height, int16 Luma, int16 Blue, int16 Red, int32 QuantScale = 2)
	{
		FMdecPicture Picture;
		Picture.Width = Width;
		Picture.Height = Height;
		Picture.Luma.assign(static_cast<std::size_t>(Width * Height), Luma);
		Picture.ChromaBlue.assign(static_cast<std::size_t>(Width * Height / 4), Blue);
		Picture.ChromaRed.assign(static_cast<std::size_t>(Width * Height / 4), Red);
		Picture.QuantScale = QuantScale;
		return Picture;
	}

	FMovieImage ImageOf(int32 Width, int32 Height, auto&& Luma)
	{
		FMovieImage Image{Width, Height, std::vector<FMovieTexel>(static_cast<std::size_t>(Width * Height))};
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const int16 Value = static_cast<int16>(Luma(X, Y));
				Image.Texels[static_cast<std::size_t>(Y * Width + X)] = {Value, static_cast<int16>(Value / 2), static_cast<int16>(-Value / 3), 0};
			}
		}
		return Image;
	}

	int16& LumaAt(FMdecPicture& Picture, int32 X, int32 Y)
	{
		return Picture.Luma[static_cast<std::size_t>(Y * Picture.Width + X)];
	}

	int32 LumaAt(const FMdecPicture& Picture, int32 X, int32 Y)
	{
		return Picture.Luma[static_cast<std::size_t>(Y * Picture.Width + X)];
	}
}

TEST_CASE("A level step at a block edge is spread over the blocks, a real edge kept", "[Graphics]")
{
	// Two flat blocks a level apart at quantization scale 2: the step is the coding's, spread over the eight samples.
	FMdecPicture Picture = FlatPicture(32, 16, 0, 0, 0);
	for (int32 Y = 0; Y < 16; ++Y)
	{
		for (int32 X = 8; X < 32; ++X)
		{
			LumaAt(Picture, X, Y) = static_cast<int16>(X < 16 ? LevelSize : 0);
		}
	}
	CleanMoviePicture(Picture);
	for (int32 X = 4; X < 12; ++X)
	{
		INFO("x " << X);
		CHECK(LumaAt(Picture, X, 5) <= LumaAt(Picture, X + 1, 5));
	}
	CHECK(LumaAt(Picture, 8, 5) - LumaAt(Picture, 7, 5) < LevelSize / 2);

	// A hundred levels at the same edge is a picture's edge: kept.
	FMdecPicture Edge = FlatPicture(32, 16, 0, 0, 0);
	for (int32 Y = 0; Y < 16; ++Y)
	{
		for (int32 X = 8; X < 32; ++X)
		{
			LumaAt(Edge, X, Y) = static_cast<int16>(100 * LevelSize);
		}
	}
	const std::vector<int16> Before = Edge.Luma;
	CleanMoviePicture(Edge);
	CHECK(Edge.Luma == Before);
}

TEST_CASE("Ripples beside an edge inside a block are smoothed, by no more than half the quantizer", "[Graphics]")
{
	FMdecPicture Picture = FlatPicture(16, 16, 0, 0, 0, 4);
	for (int32 Y = 0; Y < 16; ++Y)
	{
		for (int32 X = 0; X < 16; ++X)
		{
			const int32 Ripple = ((X + Y) % 2 == 0 ? 1 : -1) * LevelSize;
			LumaAt(Picture, X, Y) = static_cast<int16>((X % 8 < 4 ? 0 : 80 * LevelSize) + Ripple);
		}
	}
	const FMdecPicture Before = Picture;
	CleanMoviePicture(Picture);
	int32 RippleBefore = 0;
	int32 RippleAfter = 0;
	for (int32 Y = 2; Y < 6; ++Y)
	{
		RippleBefore += std::abs(LumaAt(Before, 1, Y) - LumaAt(Before, 1, Y + 1));
		RippleAfter += std::abs(LumaAt(Picture, 1, Y) - LumaAt(Picture, 1, Y + 1));
	}
	CHECK(RippleAfter < RippleBefore / 2);
	for (std::size_t Index = 0; Index < Picture.Luma.size(); ++Index)
	{
		CHECK(std::abs(Picture.Luma[Index] - Before.Luma[Index]) <= 2 * LevelSize);
	}
	// The edge itself stays where it was, as sharp.
	CHECK(LumaAt(Picture, 4, 8) - LumaAt(Picture, 3, 8) > 75 * LevelSize);
}

TEST_CASE("A flat movie picture upscales to its own colour everywhere", "[Graphics]")
{
	const FMdecPicture Picture = FlatPicture(32, 32, 40 * LevelSize, -20 * LevelSize, 30 * LevelSize);
	const FMovieImage Upscaled = MovieUpscale::Upscale(Picture);
	REQUIRE(Upscaled.Width == 32 * MovieUpscale::Factor);
	REQUIRE(Upscaled.Height == 32 * MovieUpscale::Factor);
	const FMovieTexel Expected = {40 * LevelSize, -20 * LevelSize, 30 * LevelSize, 0};
	CHECK(std::all_of(Upscaled.Texels.begin(), Upscaled.Texels.end(), [&](const FMovieTexel& Texel) { return Texel == Expected; }));

	const std::vector<uint8> Converted = ToRgb24(Picture);
	const MovieUpscale::FPlacement Placement{1280, 960, 32, 24, 0, 2, 32, 20};
	std::array<uint8, 3> Rgb{};
	REQUIRE(MovieUpscale::DisplayPixel(Upscaled, Placement, 640, 480, Rgb));
	CHECK(Rgb == std::array<uint8, 3>{Converted[0], Converted[1], Converted[2]});
	// The screen's top two lines and those under the picture's twenty are not the picture's.
	CHECK_FALSE(MovieUpscale::DisplayPixel(Upscaled, Placement, 640, 79, Rgb));
	CHECK(MovieUpscale::DisplayPixel(Upscaled, Placement, 640, 80, Rgb));
	CHECK(MovieUpscale::DisplayPixel(Upscaled, Placement, 640, 879, Rgb));
	CHECK_FALSE(MovieUpscale::DisplayPixel(Upscaled, Placement, 640, 880, Rgb));
}

TEST_CASE("Doubling keeps the image's pixels at the even places and stays within the image's range", "[Graphics]")
{
	Testing::FXorShift32 Random(7);
	const FMovieImage Image = ImageOf(24, 20, [&](int32, int32) { return static_cast<int32>(Random.Below(4001)) - 2000; });
	const FMovieImage Doubled = MovieUpscale::Double(Image, 2);
	REQUIRE(Doubled.Width == 48);
	REQUIRE(Doubled.Height == 40);
	for (int32 Y = 0; Y < Image.Height; ++Y)
	{
		for (int32 X = 0; X < Image.Width; ++X)
		{
			CHECK(Doubled.At(2 * X, 2 * Y) == Image.At(X, Y));
		}
	}
	for (std::size_t Channel = 0; Channel < 3; ++Channel)
	{
		const auto Less = [&](const FMovieTexel& A, const FMovieTexel& B) { return A[Channel] < B[Channel]; };
		CHECK((*std::min_element(Doubled.Texels.begin(), Doubled.Texels.end(), Less))[Channel] >=
			(*std::min_element(Image.Texels.begin(), Image.Texels.end(), Less))[Channel]);
		CHECK((*std::max_element(Doubled.Texels.begin(), Doubled.Texels.end(), Less))[Channel] <=
			(*std::max_element(Image.Texels.begin(), Image.Texels.end(), Less))[Channel]);
	}
}

TEST_CASE("Doubling draws a diagonal edge along itself, without steps", "[Graphics]")
{
	// An edge along the rising diagonal: every pixel of the doubled image away from its borders takes the value
	// its diagonal line takes, as the edge's own lines run.
	const FMovieImage Image = ImageOf(32, 32, [](int32 X, int32 Y) { return X + Y < 32 ? -800 : 800; });
	const FMovieImage Doubled = MovieUpscale::Double(Image, 1);
	for (int32 Y = 12; Y < 52; ++Y)
	{
		for (int32 X = 12; X < 52; ++X)
		{
			INFO(X << "," << Y);
			CHECK(Doubled.At(X, Y)[0] == Doubled.At(X + 1, Y - 1)[0]);
		}
	}
}

TEST_CASE("The chroma brought to every pixel stops at the luma's edge", "[Graphics]")
{
	// Dark on the left eight columns with red chroma, bright beyond with blue: the column just left of the edge
	// is red, where a plain interpolation would mix in a quarter of the blue.
	FMdecPicture Picture = FlatPicture(32, 16, 0, 0, 0);
	for (int32 Y = 0; Y < 16; ++Y)
	{
		for (int32 X = 0; X < 32; ++X)
		{
			LumaAt(Picture, X, Y) = static_cast<int16>(X < 8 ? -60 * LevelSize : 60 * LevelSize);
		}
	}
	for (int32 Y = 0; Y < 8; ++Y)
	{
		for (int32 X = 0; X < 16; ++X)
		{
			Picture.ChromaRed[static_cast<std::size_t>(Y * 16 + X)] = static_cast<int16>(X < 4 ? 50 * LevelSize : -50 * LevelSize);
		}
	}
	const FMovieImage Image = MovieUpscale::GuideChroma(Picture);
	CHECK(Image.At(7, 6)[2] > 45 * LevelSize);
	CHECK(Image.At(8, 6)[2] < -45 * LevelSize);
	CHECK(Image.At(7, 6)[0] == -60 * LevelSize);
}

TEST_CASE("A small light keeps the colour coded for it, though its luma is like none of its chroma samples", "[Graphics]")
{
	// One bright pixel on black, its 2x2's chroma sample red: the dark three of the 2x2 stand for the mean, which the
	// bright one is far from, and the black samples about them stand for black. The 2x2's colour stays the sample's,
	// within the quantization's noise.
	FMdecPicture Picture = FlatPicture(16, 16, -128 * LevelSize, 0, 0, 1);
	LumaAt(Picture, 6, 6) = static_cast<int16>(-15 * LevelSize);
	Picture.ChromaRed[3 * 8 + 3] = static_cast<int16>(27 * LevelSize);
	const FMovieImage Image = MovieUpscale::GuideChroma(Picture);
	const int32 Mean = (Image.At(6, 6)[2] + Image.At(7, 6)[2] + Image.At(6, 7)[2] + Image.At(7, 7)[2]) / 4;
	CHECK(Mean >= 27 * LevelSize - MovieUpscale::ChromaNoise(1));
	CHECK(Image.At(6, 6)[2] > 20 * LevelSize);
}

TEST_CASE("The edge sharpening narrows a soft edge without overshoot", "[Graphics]")
{
	// A smooth step of 600 sixteenths over twelve pixels, as an upscaled edge lies.
	const auto Step = [](int32 X, int32) {
		const double T = std::clamp((X - 26) / 12.0, 0.0, 1.0);
		return static_cast<int32>(std::lround(-300.0 + 600.0 * T * T * (3.0 - 2.0 * T)));
	};
	const FMovieImage Image = ImageOf(64, 16, Step);
	const FMovieImage Sharpened = MovieUpscale::SharpenEdges(Image);
	const auto Between = [](const FMovieImage& Of) {
		int32 Count = 0;
		for (int32 X = 0; X < Of.Width; ++X)
		{
			Count += std::abs(Of.At(X, 8)[0]) < 225 ? 1 : 0;
		}
		return Count;
	};
	CHECK(Between(Sharpened) < Between(Image));
	for (const FMovieTexel& Texel : Sharpened.Texels)
	{
		CHECK(std::abs(Texel[0]) <= 300);
	}
}
