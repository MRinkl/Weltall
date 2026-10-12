#pragma once

#include "Graphics/Mdec.h"

#include <array>
#include <vector>

namespace Weltall
{
	/** A pixel of FMovieImage: Y, Cb and Cr with the MDEC's fraction bits, then a zero, as a four-channel texture holds them. */
	using FMovieTexel = std::array<int16, 4>;

	/** A movie picture with its colour at every pixel, row by row. */
	struct FMovieImage
	{
		int32 Width = 0;
		int32 Height = 0;
		std::vector<FMovieTexel> Texels;

		const FMovieTexel& At(int32 X, int32 Y) const { return Texels[static_cast<std::size_t>(Y) * static_cast<std::size_t>(Width) + static_cast<std::size_t>(X)]; }
	};

	/**
	 * The movie upscaler, in integer arithmetic so that every machine gives the same pixels; the GPU runs the
	 * same steps (MovieUpscale.usf), and these are what its output is held to. Each step's rule is with it below.
	 */
	namespace MovieUpscale
	{
		/** How many times wider and taller Upscale makes a picture. */
		inline constexpr int32 Factor = 4;

		/** The luma change across a 4x4 neighbourhood too small to tell a direction by, for a picture of QuantScale. */
		WELTALLCORE_API int32 NoiseFloor(int32 QuantScale);

		/** How far a chroma sample of a picture of QuantScale may lie from its pixels' colour by its quantization alone. */
		WELTALLCORE_API int32 ChromaNoise(int32 QuantScale);

		/**
		 * The chroma brought to every pixel. First each pixel weighs the 4x4 chroma samples about it by their
		 * distance and by how near the luma each sample stands for (the mean of its 2x2 pixels) is to the
		 * pixel's own, so colour stops where the luma has an edge instead of bleeding across it. Then each
		 * 2x2's mean is brought back towards the sample coded for it by as much as it strayed past ChromaNoise:
		 * colour the luma does not show, a small light's or a glow's, keeps what the picture gave it.
		 */
		WELTALLCORE_API FMovieImage GuideChroma(const FMdecPicture& Picture);

		/**
		 * Twice the size, the image's pixels at the even places: first the pixels between four diagonally,
		 * then the rest between four across and down, each a cubic along the direction the luma changes
		 * least across a 4x4 neighbourhood, or a blend of the two directions as their changes come
		 * close, kept within its four nearest pixels. Changes below the noise of QuantScale's
		 * quantization tell no direction. Every channel takes the luma's choice.
		 */
		WELTALLCORE_API FMovieImage Double(const FMovieImage& Image, int32 QuantScale);

		/**
		 * Narrows the edges without overshoot: each pixel takes the image's colour from a little way down the
		 * slope of the luma's blurred edge strength, away from an edge's middle.
		 */
		WELTALLCORE_API FMovieImage SharpenEdges(const FMovieImage& Image);

		/**
		 * GuideChroma, Double until Factor, SharpenEdges: the picture Factor times wider and taller. The movie filter
		 * cleans a picture first (CleanMoviePicture).
		 */
		WELTALLCORE_API FMovieImage Upscale(const FMdecPicture& Picture);

		/** Where an upscaled picture lands on the screen: the screen's rectangle on the display and the picture's place on the screen. */
		struct FPlacement
		{
			/** The display's pixels the game screen fills. */
			int32 DisplayWidth = 0;
			int32 DisplayHeight = 0;
			/** The game screen's size in its own pixels. */
			int32 ScreenWidth = 0;
			int32 ScreenHeight = 0;
			/** The picture's shown part on the game screen: its top left there, and how much of the picture shows. */
			int32 X = 0;
			int32 Y = 0;
			int32 Width = 0;
			int32 Height = 0;
		};

		/**
		 * The display pixel at Column, Row of the screen's rectangle as 8-bit R, G, B: the upscaled image
		 * resampled by a Catmull-Rom, converted as the MDEC converts. False where the picture does not reach.
		 */
		WELTALLCORE_API bool DisplayPixel(const FMovieImage& Upscaled, const FPlacement& Placement, int32 Column, int32 Row, std::array<uint8, 3>& Rgb);
	}
}
