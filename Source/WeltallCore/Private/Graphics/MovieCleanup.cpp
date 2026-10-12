#include "Graphics/MovieCleanup.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace Weltall
{
	namespace
	{
		constexpr int32 BlockSize = 8;
		/** One step of 8-bit colour in the picture's samples. */
		constexpr int32 Level = 1 << MdecSampleFractionBits;
		/** Neighbours closer than this lie level. */
		constexpr int32 LevelStep = 2 * Level;
		/** Of the nine steps across a block edge, how many must be level for the edge to be smoothed over its width. */
		constexpr int32 LevelSteps = 6;
		/** The smoothing across a level edge, over the four samples each side of each sample. */
		constexpr int32 EdgeTaps[9] = {1, 1, 2, 2, 4, 2, 2, 1, 1};

		struct FPlane
		{
			int16* Samples;
			int32 Width;
			int32 Height;

			int16& At(int32 X, int32 Y) const { return Samples[static_cast<std::ptrdiff_t>(Y) * Width + X]; }
		};

		/** Divides by a power of two, rounding halves away from zero, so that a value and its negation round alike. */
		int32 DivideRounded(int32 Value, int32 Shift)
		{
			const int32 Magnitude = (std::abs(Value) + (1 << (Shift - 1))) >> Shift;
			return Value < 0 ? -Magnitude : Magnitude;
		}

		/** The jump at the middle of four samples less what the samples beside it lead to, eight times over. */
		int32 MiddleJump(const int32* V)
		{
			return 2 * V[0] - 5 * V[1] + 5 * V[2] - 2 * V[3];
		}

		/** Whether a line of ten samples bends by a level step or more anywhere but at its middle two. */
		bool BendsAwayFromMiddle(const int32* V)
		{
			for (const int32 Index : {1, 2, 3, 6, 7, 8})
			{
				if (std::abs(V[Index - 1] - 2 * V[Index] + V[Index + 1]) >= LevelStep)
				{
					return true;
				}
			}
			return false;
		}

		/**
		 * Filters one line across a block edge: the ten samples Step apart from Line straddle it, the edge
		 * between the fifth and the sixth. Where the line is level or an even slope, the edge's step is spread over
		 * the eight middle samples, each moved by no more than the quantizer; elsewhere the two samples at the edge
		 * are brought together by as much as the step exceeds the slopes beside it, as long as the step is
		 * one the quantizer could have made.
		 */
		void FilterAcrossEdge(int16* Line, std::ptrdiff_t Step, int32 Quant)
		{
			int32 V[10];
			int32 Min = 0x7FFF;
			int32 Max = -0x8000;
			int32 LevelCount = 0;
			for (int32 Index = 0; Index < 10; ++Index)
			{
				V[Index] = Line[Index * Step];
				if (Index > 0)
				{
					LevelCount += std::abs(V[Index] - V[Index - 1]) < LevelStep ? 1 : 0;
				}
				if (Index >= 1 && Index <= 8)
				{
					Min = std::min(Min, V[Index]);
					Max = std::max(Max, V[Index]);
				}
			}
			if (LevelCount >= LevelSteps)
			{
				// MPEG-4 smooths a level line whose samples span less than twice the quantizer. One sloping evenly,
				// as a gradient does, may span more, as long as its step at the edge is the quantizer's and it bends
				// nowhere else: a small light or a fine line beside the edge is the picture's own.
				const bool bNarrow = Max - Min < 2 * Quant;
				const bool bEvenSlope = std::abs(V[4] - V[5]) < Quant && !BendsAwayFromMiddle(V);
				if (!bNarrow && !bEvenSlope)
				{
					return;
				}
				// Beyond the eight samples a line continues with its outer sample, unless it steps away there.
				const int32 First = std::abs(V[1] - V[0]) < Quant ? V[0] : V[1];
				const int32 Last = std::abs(V[8] - V[9]) < Quant ? V[9] : V[8];
				int32 Extended[16];
				std::fill_n(Extended, 4, First);
				std::copy(V + 1, V + 9, Extended + 4);
				std::fill_n(Extended + 12, 4, Last);
				for (int32 Index = 1; Index <= 8; ++Index)
				{
					int32 Sum = 0;
					for (int32 Tap = 0; Tap < 9; ++Tap)
					{
						Sum += EdgeTaps[Tap] * Extended[Index - 1 + Tap];
					}
					const int32 Smoothed = (Sum + 8) >> 4;
					Line[Index * Step] = static_cast<int16>(V[Index] + std::clamp(Smoothed - V[Index], -Quant, Quant));
				}
				return;
			}
			const int32 Jump = MiddleJump(V + 3);
			if (std::abs(Jump) >= 8 * Quant)
			{
				return;
			}
			const int32 Least = std::min({std::abs(Jump), std::abs(MiddleJump(V + 1)), std::abs(MiddleJump(V + 5))});
			const int32 Kept = Jump < 0 ? -Least : Least;
			const int32 Half = (V[4] - V[5]) / 2;
			const int32 Move = DivideRounded(5 * (Kept - Jump), 6);
			const int32 Bounded = Half >= 0 ? std::clamp(Move, 0, Half) : std::clamp(Move, Half, 0);
			Line[4 * Step] = static_cast<int16>(V[4] - Bounded);
			Line[5 * Step] = static_cast<int16>(V[5] + Bounded);
		}

		/** Filters every edge between the plane's blocks: across the columns' edges first, then across the rows'. */
		void Deblock(const FPlane& Plane, int32 Quant)
		{
			for (int32 Y = 0; Y < Plane.Height; ++Y)
			{
				for (int32 X = BlockSize; X + 5 <= Plane.Width; X += BlockSize)
				{
					FilterAcrossEdge(&Plane.At(X - 5, Y), 1, Quant);
				}
			}
			for (int32 Y = BlockSize; Y + 5 <= Plane.Height; Y += BlockSize)
			{
				for (int32 X = 0; X < Plane.Width; ++X)
				{
					FilterAcrossEdge(&Plane.At(X, Y - 5), Plane.Width, Quant);
				}
			}
		}

		/**
		 * In each block with an edge in it (its samples spanning more than twice the quantizer), smooths the
		 * samples whose whole neighbourhood lies on one side of the block's middle level, each by no more than
		 * half the quantizer: the ripples beside the edge go, the edge stays.
		 */
		void Dering(const FPlane& Plane, int32 Quant)
		{
			// The plane as it was, bordered by its edge samples so that every neighbourhood lies inside.
			const int32 Stride = Plane.Width + 2;
			std::vector<int16> Before(static_cast<std::size_t>(Stride * (Plane.Height + 2)));
			for (int32 Y = -1; Y <= Plane.Height; ++Y)
			{
				for (int32 X = -1; X <= Plane.Width; ++X)
				{
					Before[static_cast<std::size_t>((Y + 1) * Stride + X + 1)] = Plane.At(std::clamp(X, 0, Plane.Width - 1), std::clamp(Y, 0, Plane.Height - 1));
				}
			}
			// A sample's neighbourhood from its top-left neighbour, which sits at the sample's own place in Before.
			const auto Neighbourhood = [&](int32 X, int32 Y) { return Before.data() + static_cast<std::ptrdiff_t>(Y) * Stride + X; };
			for (int32 Top = 0; Top < Plane.Height; Top += BlockSize)
			{
				for (int32 Left = 0; Left < Plane.Width; Left += BlockSize)
				{
					int32 Min = 0x7FFF;
					int32 Max = -0x8000;
					for (int32 Y = 1; Y <= BlockSize; ++Y)
					{
						const int16* Row = Neighbourhood(Left + 1, Top + Y);
						for (int32 X = 0; X < BlockSize; ++X)
						{
							Min = std::min<int32>(Min, Row[X]);
							Max = std::max<int32>(Max, Row[X]);
						}
					}
					if (Max - Min < 2 * Quant)
					{
						continue;
					}
					const int32 Middle = (Max + Min + 1) >> 1;
					// Which samples of the block and its border lie at or above the middle, counted three across.
					int32 AboveAcross[BlockSize + 2][BlockSize] = {};
					for (int32 Y = 0; Y < BlockSize + 2; ++Y)
					{
						const int16* Row = Neighbourhood(Left, Top + Y);
						for (int32 X = 0; X < BlockSize; ++X)
						{
							AboveAcross[Y][X] = (Row[X] >= Middle) + (Row[X + 1] >= Middle) + (Row[X + 2] >= Middle);
						}
					}
					for (int32 Y = 0; Y < BlockSize; ++Y)
					{
						for (int32 X = 0; X < BlockSize; ++X)
						{
							const int32 Above = AboveAcross[Y][X] + AboveAcross[Y + 1][X] + AboveAcross[Y + 2][X];
							if (Above != 0 && Above != 9)
							{
								continue;
							}
							const int16* First = Neighbourhood(Left + X, Top + Y);
							const int16* Second = First + Stride;
							const int16* Third = Second + Stride;
							const int32 Sum = First[0] + 2 * First[1] + First[2] + 2 * Second[0] + 4 * Second[1] + 2 * Second[2] + Third[0] + 2 * Third[1] + Third[2];
							const int32 Sample = Second[1];
							Plane.At(Left + X, Top + Y) = static_cast<int16>(Sample + std::clamp(((Sum + 8) >> 4) - Sample, -Quant / 2, Quant / 2));
						}
					}
				}
			}
		}
	}

	void CleanMoviePicture(FMdecPicture& Picture)
	{
		const int32 Quant = Picture.QuantScale * Level;
		const FPlane Planes[3] = {
			{Picture.Luma.data(), Picture.Width, Picture.Height},
			{Picture.ChromaBlue.data(), Picture.Width / 2, Picture.Height / 2},
			{Picture.ChromaRed.data(), Picture.Width / 2, Picture.Height / 2},
		};
		for (const FPlane& Plane : Planes)
		{
			Deblock(Plane, Quant);
			Dering(Plane, Quant);
		}
	}
}
