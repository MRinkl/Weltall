#include "Graphics/MovieUpscale.h"

#include <algorithm>
#include <cstdlib>

namespace Weltall::MovieUpscale
{
	namespace
	{
		constexpr int32 LevelBits = MdecSampleFractionBits;

		/** A chroma sample's weight by its distance from a pixel's centre, for the 4 samples about the pixel: on an even row or column the distances are 1.75, 0.75, 0.25 and 1.25 samples, on an odd one the reverse. */
		constexpr int32 EvenTaps[4] = {1, 10, 15, 5};
		constexpr int32 OddTaps[4] = {5, 15, 10, 1};
		/** Chroma beyond 256 levels either way counts as at it, keeping the weighted sums within 32 bits. */
		constexpr int32 ChromaLimit = (256 << LevelBits) - 1;
		/** The luma difference at which a chroma sample's weight halves, squared: ten levels. */
		constexpr int32 GuideSpreadSquared = (10 << LevelBits) * (10 << LevelBits);
		/** Luma differences past 64 levels count as 64. */
		constexpr int32 GuideDifferenceLimit = (64 << LevelBits) - 1;

		constexpr int32 WeightBits = 12;
		constexpr int32 WeightOne = 1 << WeightBits;
		/** The luma change across a 4x4 neighbourhood that tells no direction, per step of quantization scale: two levels on each of the nine differences summed. */
		constexpr int32 NoiseFloorPerScale = 9 * (2 << LevelBits);

		/** The edge strength's ceiling: 64 levels of the Sobel operator's response. */
		constexpr int32 EdgeLimit = 64 << LevelBits;
		/** A pixel's displacement in sixteenths of a pixel is the edge strength's slope over this, two pixels at most. */
		constexpr int32 WarpDivisor = 16;
		constexpr int32 WarpLimit = 32;

		/** The four luma differences summed along each direction, as pairs of offsets (X, Y, X, Y) about the pixel to fill. */
		struct FPairs
		{
			int32 Offsets[9][4];
		};
		/** Between pixels known two apart across a row, about a pixel whose row and column differ in parity. */
		constexpr FPairs AcrossPairs = {{{-1, 0, 1, 0}, {0, -1, 2, -1}, {-2, -1, 0, -1}, {0, 1, 2, 1}, {-2, 1, 0, 1}, {1, 0, 3, 0}, {-3, 0, -1, 0},
			{-1, -2, 1, -2}, {-1, 2, 1, 2}}};
		constexpr FPairs DownPairs = {{{0, -1, 0, 1}, {-1, 0, -1, 2}, {-1, -2, -1, 0}, {1, 0, 1, 2}, {1, -2, 1, 0}, {0, 1, 0, 3}, {0, -3, 0, -1},
			{-2, -1, -2, 1}, {2, -1, 2, 1}}};

		int32 Clamp(int32 Index, int32 Size)
		{
			return std::clamp(Index, 0, Size - 1);
		}

		/** An index reflected back into 0..Size-1 about its first and last places: its parity kept, so on a doubled grid a known place reflects to a known place. */
		int32 Mirror(int32 Index, int32 Size)
		{
			return Index < 0 ? -Index : (Index >= Size ? 2 * Size - 2 - Index : Index);
		}

		/** Numerator over a positive Denominator, rounded, halves away from zero. */
		int32 DivideRounded(int32 Numerator, int32 Denominator)
		{
			const uint32 Magnitude = (static_cast<uint32>(std::abs(Numerator)) + static_cast<uint32>(Denominator / 2)) / static_cast<uint32>(Denominator);
			return Numerator < 0 ? -static_cast<int32>(Magnitude) : static_cast<int32>(Magnitude);
		}

		/** How much a chroma sample counts, 1 to 32, by the difference between the luma it stands for and the pixel's. */
		int32 Likeness(int32 Difference)
		{
			const uint32 Ratio = (static_cast<uint32>(Difference * Difference) << 8) / GuideSpreadSquared;
			const uint32 RatioSquared = (Ratio * Ratio) >> 8;
			return 1 + static_cast<int32>((31u << 8) / (256u + RatioSquared));
		}

		/** Value moved towards zero by Amount, stopping there. */
		int32 Shrink(int32 Value, int32 Amount)
		{
			return Value > Amount ? Value - Amount : (Value < -Amount ? Value + Amount : 0);
		}

		int32 FifthPower(int32 Fraction)
		{
			const int32 Square = (Fraction * Fraction) >> WeightBits;
			const int32 Fourth = (Square * Square) >> WeightBits;
			return (Fourth * Fraction) >> WeightBits;
		}

		/** The weight, in WeightOne, of interpolating along direction A, given the luma's change along A and along B: the less change, the more weight. */
		int32 DirectionWeight(int32 ChangeA, int32 ChangeB, int32 Floor)
		{
			const uint32 ForA = static_cast<uint32>(ChangeB + Floor);
			const uint32 Total = static_cast<uint32>(ChangeA + ChangeB + 2 * Floor);
			const int32 ShareA = static_cast<int32>((ForA << WeightBits) / Total);
			const int32 PowerA = FifthPower(ShareA);
			const int32 PowerB = FifthPower(WeightOne - ShareA);
			return static_cast<int32>((static_cast<uint32>(PowerA) << WeightBits) / static_cast<uint32>(PowerA + PowerB));
		}

		int32 Cubic(int32 A, int32 B, int32 C, int32 D)
		{
			return (-A + 9 * B + 9 * C - D + 8) >> 4;
		}

		int32 Blend(int32 Weight, int32 A, int32 B)
		{
			return (Weight * A + (WeightOne - Weight) * B + WeightOne / 2) >> WeightBits;
		}

		int32 Within(int32 Value, int32 A, int32 B, int32 C, int32 D)
		{
			return std::clamp(Value, std::min({A, B, C, D}), std::max({A, B, C, D}));
		}

		template <typename TFetch>
		int32 ChangeAlong(const FPairs& Pairs, TFetch&& Fetch, int32 X, int32 Y)
		{
			int32 Sum = 0;
			for (const auto& Pair : Pairs.Offsets)
			{
				Sum += std::abs(Fetch(X + Pair[0], Y + Pair[1])[0] - Fetch(X + Pair[2], Y + Pair[3])[0]);
			}
			return Sum;
		}

		/** The Catmull-Rom weights in WeightOne for a position Fraction 256ths past the second tap. */
		std::array<int32, 4> CatmullRom(int32 Fraction)
		{
			const int32 Square = Fraction * Fraction;
			const int32 Cube = Square * Fraction;
			// In 2^-25: w0 = (-t^3 + 2t^2 - t) / 2, w2 = (-3t^3 + 4t^2 + t) / 2, w3 = (t^3 - t^2) / 2, t = Fraction / 256.
			const int32 First = DivideRounded(-Cube + 512 * Square - 65536 * Fraction, 1 << 13);
			const int32 Third = DivideRounded(-3 * Cube + 1024 * Square + 65536 * Fraction, 1 << 13);
			const int32 Fourth = DivideRounded(Cube - 256 * Square, 1 << 13);
			return {First, WeightOne - First - Third - Fourth, Third, Fourth};
		}

		uint8 ToUnsigned8(int32 Sample)
		{
			return static_cast<uint8>(std::clamp((Sample + (1 << (LevelBits - 1))) >> LevelBits, -128, 127) + 128);
		}
	}

	int32 NoiseFloor(int32 QuantScale)
	{
		return NoiseFloorPerScale * (QuantScale + 1);
	}

	int32 ChromaNoise(int32 QuantScale)
	{
		return (3 * QuantScale + 1) << LevelBits;
	}

	FMovieImage GuideChroma(const FMdecPicture& Picture)
	{
		const int32 Width = Picture.Width;
		const int32 ChromaWidth = Width / 2;
		const int32 ChromaHeight = Picture.Height / 2;
		const auto Luma = [&](int32 X, int32 Y) -> int32 { return Picture.Luma[static_cast<std::size_t>(Y * Width + X)]; };
		std::vector<int32> StandsFor(static_cast<std::size_t>(ChromaWidth * ChromaHeight));
		for (int32 Y = 0; Y < ChromaHeight; ++Y)
		{
			for (int32 X = 0; X < ChromaWidth; ++X)
			{
				StandsFor[static_cast<std::size_t>(Y * ChromaWidth + X)] =
					(Luma(2 * X, 2 * Y) + Luma(2 * X + 1, 2 * Y) + Luma(2 * X, 2 * Y + 1) + Luma(2 * X + 1, 2 * Y + 1) + 2) >> 2;
			}
		}
		FMovieImage Image{Width, Picture.Height, std::vector<FMovieTexel>(Picture.Luma.size())};
		for (int32 Y = 0; Y < Picture.Height; ++Y)
		{
			const int32* RowTaps = (Y & 1) != 0 ? OddTaps : EvenTaps;
			const int32 FirstRow = (Y >> 1) - 2 + (Y & 1);
			for (int32 X = 0; X < Width; ++X)
			{
				const int32* ColumnTaps = (X & 1) != 0 ? OddTaps : EvenTaps;
				const int32 FirstColumn = (X >> 1) - 2 + (X & 1);
				const int32 Own = Luma(X, Y);
				int32 WeightSum = 0;
				int32 BlueSum = 0;
				int32 RedSum = 0;
				for (int32 TapY = 0; TapY < 4; ++TapY)
				{
					for (int32 TapX = 0; TapX < 4; ++TapX)
					{
						const std::size_t Index = static_cast<std::size_t>(Clamp(FirstRow + TapY, ChromaHeight) * ChromaWidth + Clamp(FirstColumn + TapX, ChromaWidth));
						const int32 Weight = RowTaps[TapY] * ColumnTaps[TapX] * Likeness(std::min(std::abs(Own - StandsFor[Index]), GuideDifferenceLimit));
						WeightSum += Weight;
						BlueSum += Weight * std::clamp<int32>(Picture.ChromaBlue[Index], -ChromaLimit, ChromaLimit);
						RedSum += Weight * std::clamp<int32>(Picture.ChromaRed[Index], -ChromaLimit, ChromaLimit);
					}
				}
				Image.Texels[static_cast<std::size_t>(Y * Width + X)] = {
					static_cast<int16>(Own), static_cast<int16>(DivideRounded(BlueSum, WeightSum)), static_cast<int16>(DivideRounded(RedSum, WeightSum)), 0};
			}
		}
		const int32 Noise = ChromaNoise(Picture.QuantScale);
		FMovieImage Kept = Image;
		for (int32 Y = 0; Y < Picture.Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const std::size_t Sample = static_cast<std::size_t>((Y >> 1) * ChromaWidth + (X >> 1));
				for (std::size_t Channel = 1; Channel < 3; ++Channel)
				{
					const int32 Coded = std::clamp<int32>((Channel == 1 ? Picture.ChromaBlue : Picture.ChromaRed)[Sample], -ChromaLimit, ChromaLimit);
					const int32 Mean = (Image.At(X & ~1, Y & ~1)[Channel] + Image.At(X | 1, Y & ~1)[Channel] + Image.At(X & ~1, Y | 1)[Channel] +
						Image.At(X | 1, Y | 1)[Channel] + 2) >> 2;
					Kept.Texels[static_cast<std::size_t>(Y * Width + X)][Channel] =
						static_cast<int16>(Image.At(X, Y)[Channel] + Shrink(Coded - Mean, Noise));
				}
			}
		}
		return Kept;
	}

	FMovieImage Double(const FMovieImage& Image, int32 QuantScale)
	{
		const int32 Floor = NoiseFloor(QuantScale);
		const int32 Width = Image.Width * 2;
		const int32 Height = Image.Height * 2;
		FMovieImage Doubled{Width, Height, std::vector<FMovieTexel>(static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height))};
		const auto Set = [&](int32 X, int32 Y) -> FMovieTexel& { return Doubled.Texels[static_cast<std::size_t>(Y * Width + X)]; };
		const auto Source = [&](int32 X, int32 Y) -> const FMovieTexel& { return Image.At(Mirror(X, Image.Width), Mirror(Y, Image.Height)); };
		for (int32 Y = 0; Y < Image.Height; ++Y)
		{
			for (int32 X = 0; X < Image.Width; ++X)
			{
				Set(2 * X, 2 * Y) = Image.At(X, Y);
				int32 Rising = 0;
				int32 Falling = 0;
				for (int32 Row = -1; Row <= 1; ++Row)
				{
					for (int32 Column = -1; Column <= 1; ++Column)
					{
						Rising += std::abs(Source(X + Column, Y + Row + 1)[0] - Source(X + Column + 1, Y + Row)[0]);
						Falling += std::abs(Source(X + Column, Y + Row)[0] - Source(X + Column + 1, Y + Row + 1)[0]);
					}
				}
				const int32 Weight = DirectionWeight(Rising, Falling, Floor);
				FMovieTexel& Out = Set(2 * X + 1, 2 * Y + 1);
				for (std::size_t Channel = 0; Channel < 3; ++Channel)
				{
					const int32 AlongRising = Cubic(Source(X - 1, Y + 2)[Channel], Source(X, Y + 1)[Channel], Source(X + 1, Y)[Channel], Source(X + 2, Y - 1)[Channel]);
					const int32 AlongFalling = Cubic(Source(X - 1, Y - 1)[Channel], Source(X, Y)[Channel], Source(X + 1, Y + 1)[Channel], Source(X + 2, Y + 2)[Channel]);
					Out[Channel] = static_cast<int16>(Within(Blend(Weight, AlongRising, AlongFalling), Source(X, Y)[Channel], Source(X + 1, Y)[Channel],
						Source(X, Y + 1)[Channel], Source(X + 1, Y + 1)[Channel]));
				}
			}
		}
		const auto Known = [&](int32 X, int32 Y) -> const FMovieTexel& { return Doubled.At(Mirror(X, Width), Mirror(Y, Height)); };
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 1 - (Y & 1); X < Width; X += 2)
			{
				const int32 Weight = DirectionWeight(ChangeAlong(AcrossPairs, Known, X, Y), ChangeAlong(DownPairs, Known, X, Y), Floor);
				FMovieTexel& Out = Set(X, Y);
				for (std::size_t Channel = 0; Channel < 3; ++Channel)
				{
					const int32 Left = Known(X - 1, Y)[Channel];
					const int32 Right = Known(X + 1, Y)[Channel];
					const int32 Up = Known(X, Y - 1)[Channel];
					const int32 Down = Known(X, Y + 1)[Channel];
					const int32 Across = Cubic(Known(X - 3, Y)[Channel], Left, Right, Known(X + 3, Y)[Channel]);
					const int32 Downward = Cubic(Known(X, Y - 3)[Channel], Up, Down, Known(X, Y + 3)[Channel]);
					Out[Channel] = static_cast<int16>(Within(Blend(Weight, Across, Downward), Left, Right, Up, Down));
				}
			}
		}
		return Doubled;
	}

	FMovieImage SharpenEdges(const FMovieImage& Image)
	{
		const int32 Width = Image.Width;
		const int32 Height = Image.Height;
		const auto Index = [&](int32 X, int32 Y) { return static_cast<std::size_t>(Clamp(Y, Height) * Width + Clamp(X, Width)); };
		const auto Luma = [&](int32 X, int32 Y) -> int32 { return Image.Texels[Index(X, Y)][0]; };
		std::vector<int32> Strength(Image.Texels.size());
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const int32 Across = Luma(X + 1, Y - 1) + 2 * Luma(X + 1, Y) + Luma(X + 1, Y + 1) - Luma(X - 1, Y - 1) - 2 * Luma(X - 1, Y) - Luma(X - 1, Y + 1);
				const int32 Down = Luma(X - 1, Y + 1) + 2 * Luma(X, Y + 1) + Luma(X + 1, Y + 1) - Luma(X - 1, Y - 1) - 2 * Luma(X, Y - 1) - Luma(X + 1, Y - 1);
				Strength[Index(X, Y)] = std::min(std::abs(Across) + std::abs(Down), EdgeLimit);
			}
		}
		// Blurred by 1 4 6 4 1 across, then down.
		std::vector<int32> Across(Strength.size());
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				Across[Index(X, Y)] = (Strength[Index(X - 2, Y)] + 4 * Strength[Index(X - 1, Y)] + 6 * Strength[Index(X, Y)] + 4 * Strength[Index(X + 1, Y)] +
					Strength[Index(X + 2, Y)] + 8) >> 4;
			}
		}
		std::vector<int32> Blurred(Strength.size());
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				Blurred[Index(X, Y)] = (Across[Index(X, Y - 2)] + 4 * Across[Index(X, Y - 1)] + 6 * Across[Index(X, Y)] + 4 * Across[Index(X, Y + 1)] +
					Across[Index(X, Y + 2)] + 8) >> 4;
			}
		}
		FMovieImage Sharpened{Width, Height, std::vector<FMovieTexel>(Image.Texels.size())};
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const int32 ShiftX = std::clamp(-DivideRounded(Blurred[Index(X + 1, Y)] - Blurred[Index(X - 1, Y)], WarpDivisor), -WarpLimit, WarpLimit);
				const int32 ShiftY = std::clamp(-DivideRounded(Blurred[Index(X, Y + 1)] - Blurred[Index(X, Y - 1)], WarpDivisor), -WarpLimit, WarpLimit);
				const int32 FromX = X * 16 + ShiftX;
				const int32 FromY = Y * 16 + ShiftY;
				const int32 Left = FromX >> 4;
				const int32 Top = FromY >> 4;
				const int32 FractionX = FromX & 15;
				const int32 FractionY = FromY & 15;
				const FMovieTexel& A = Image.Texels[Index(Left, Top)];
				const FMovieTexel& B = Image.Texels[Index(Left + 1, Top)];
				const FMovieTexel& C = Image.Texels[Index(Left, Top + 1)];
				const FMovieTexel& D = Image.Texels[Index(Left + 1, Top + 1)];
				FMovieTexel& Out = Sharpened.Texels[Index(X, Y)];
				for (std::size_t Channel = 0; Channel < 3; ++Channel)
				{
					Out[Channel] = static_cast<int16>((A[Channel] * (16 - FractionX) * (16 - FractionY) + B[Channel] * FractionX * (16 - FractionY) +
						C[Channel] * (16 - FractionX) * FractionY + D[Channel] * FractionX * FractionY + 128) >> 8);
				}
			}
		}
		return Sharpened;
	}

	FMovieImage Upscale(const FMdecPicture& Picture)
	{
		FMovieImage Image = GuideChroma(Picture);
		for (int32 Doubling = 1; Doubling < Factor; Doubling *= 2)
		{
			Image = Double(Image, Picture.QuantScale);
		}
		return SharpenEdges(Image);
	}

	bool DisplayPixel(const FMovieImage& Upscaled, const FPlacement& Placement, int32 Column, int32 Row, std::array<uint8, 3>& Rgb)
	{
		// The display pixel's centre on the picture, in 256ths of its pixels.
		const int32 PictureX = (2 * Column + 1) * Placement.ScreenWidth * 128 / Placement.DisplayWidth - Placement.X * 256;
		const int32 PictureY = (2 * Row + 1) * Placement.ScreenHeight * 128 / Placement.DisplayHeight - Placement.Y * 256;
		if (PictureX < 0 || PictureY < 0 || PictureX >= Placement.Width * 256 || PictureY >= Placement.Height * 256)
		{
			return false;
		}
		// A picture pixel's centre lies on the upscaled image's pixel Factor times its index.
		const int32 GridX = Factor * (PictureX - 128);
		const int32 GridY = Factor * (PictureY - 128);
		const std::array<int32, 4> WeightsX = CatmullRom(GridX & 255);
		const std::array<int32, 4> WeightsY = CatmullRom(GridY & 255);
		std::array<int32, 3> Sample{};
		for (std::size_t Channel = 0; Channel < 3; ++Channel)
		{
			int32 Sum = 0;
			for (int32 TapY = 0; TapY < 4; ++TapY)
			{
				const int32 Y = Clamp((GridY >> 8) - 1 + TapY, Upscaled.Height);
				int32 RowSum = 0;
				for (int32 TapX = 0; TapX < 4; ++TapX)
				{
					RowSum += WeightsX[static_cast<std::size_t>(TapX)] * Upscaled.At(Clamp((GridX >> 8) - 1 + TapX, Upscaled.Width), Y)[Channel];
				}
				Sum += WeightsY[static_cast<std::size_t>(TapY)] * DivideRounded(RowSum, WeightOne);
			}
			Sample[Channel] = DivideRounded(Sum, WeightOne);
		}
		// The MDEC's conversion, as ToRgb24's: BT.601 weights in 256ths.
		const int32 Luma = Sample[0];
		const int32 Blue = Sample[1];
		const int32 Red = Sample[2];
		Rgb = {ToUnsigned8(Luma + ((359 * Red + 128) >> 8)), ToUnsigned8(Luma + ((-88 * Blue - 183 * Red + 128) >> 8)), ToUnsigned8(Luma + ((454 * Blue + 128) >> 8))};
		return true;
	}
}
