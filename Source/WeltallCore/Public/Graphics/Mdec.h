#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	/** What the MDEC decodes with: the two quantization tables, in zigzag order, and the IDCT matrix. */
	struct FMdecTables
	{
		std::array<uint8, 64> LumaQuant{};
		std::array<uint8, 64> ChromaQuant{};
		/** The DCT basis, row k holding frequency k, in 1.14 fixed point with the sqrt(2) the MDEC expects. */
		std::array<int16, 64> Scale{};

		/**
		 * The tables Sony's movie library uploads: the MPEG-1 default intra matrix (its DC weight 2)
		 * for luma and chroma alike, and the standard basis.
		 */
		WELTALLCORE_API static const FMdecTables& Standard();
	};

	/**
	 * A decoded picture in YCbCr 4:2:0, the chroma planes at half resolution both ways. Samples are
	 * signed and centred on zero, with 4 fractional bits (16 is one step of 8-bit colour), and not
	 * clamped: presentation converts them at whatever precision it wants.
	 */
	struct FMdecPicture
	{
		int32 Width = 0;
		int32 Height = 0;
		std::vector<int16> Luma;
		std::vector<int16> ChromaBlue;
		std::vector<int16> ChromaRed;
		/** The coarsest quantization scale of the picture's blocks: a movie quantizes all of a picture's blocks alike. */
		int32 QuantScale = 0;
	};

	/** Fixed-point bits the picture's samples carry beyond 8-bit colour. */
	inline constexpr int32 MdecSampleFractionBits = 4;

	/**
	 * Decodes the MDEC's run-length codes into a picture of 16x16 macroblocks, taken column by column
	 * as movies store them: each is Cr, Cb, then the luma blocks top-left, top-right, bottom-left,
	 * bottom-right. The inverse DCT runs at full precision; the hardware keeps fewer bits.
	 */
	WELTALLCORE_API TResult<FMdecPicture> DecodeMdecPicture(std::span<const uint16> Codes, int32 Width, int32 Height,
		const FMdecTables& Tables);

	/**
	 * The picture as 8-bit R, G, B per pixel, converted as the MDEC does: each chroma sample covers
	 * 2x2 pixels, BT.601 weights, saturated to 8 bits.
	 */
	WELTALLCORE_API std::vector<uint8> ToRgb24(const FMdecPicture& Picture);

	/**
	 * The picture as the MDEC's 15-bit output leaves it in VRAM, row by row into Units (Width x Height
	 * of them): ToRgb24's colours kept to their top five bits, red lowest, the semi-transparency bit clear.
	 */
	WELTALLCORE_API void ToRgb15(const FMdecPicture& Picture, std::span<uint16> Units);
}
