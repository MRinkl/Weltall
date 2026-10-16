#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/Vram.h"

#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	/** Sony's TIM image: pixels for a VRAM rectangle, optionally with a CLUT for 4/8-bit images. */
	struct FTim
	{
		/** 4, 8, 16 or 24. */
		uint8 BitsPerPixel = 0;
		/** Colors as stored: 15-bit BGR with bit 15 as the semi-transparency flag. */
		std::optional<FVramImage> Clut;
		/** Each unit holds four 4-bit or two 8-bit palette indices, or direct color. */
		FVramImage Image;
		/** Bytes the TIM occupies where it is stored, header and blocks included. */
		std::size_t ByteSize = 0;

		/** Width in pixels (the VRAM rectangle counts 16-bit units). */
		int32 PixelWidth() const { return Image.Rect.Width * 16 / BitsPerPixel; }

		/** Palette index of a pixel of a 4- or 8-bit image. */
		WELTALLCORE_API uint8 IndexAt(int32 X, int32 Y) const;
	};

	/** A TIM; its image is marked as a picture (FVramImage::Picture) with its depth and its CLUT's first palette. */
	WELTALLCORE_API TResult<FTim> ParseTim(std::span<const uint8> Bytes);

	/**
	 * Every TIM within bytes of any kind, at whole words: those whose identifier and flags are a 4-, 8- or 15-bit
	 * image's (an indexed one with its CLUT) and whose blocks are exactly as long as their rectangles within VRAM.
	 */
	WELTALLCORE_API std::vector<FTim> FindTims(std::span<const uint8> Bytes);

	/**
	 * What libgpu's OpenTIM and ReadTIM hand LoadImage from TIMs stored one after another: each TIM's
	 * CLUT (when it has one), then its pixels, until a word that is not a TIM's identifier or the end
	 * of the bytes. Like ReadTIM, it looks at nothing but the CLUT flag and steps over each block by
	 * its length in whole words.
	 */
	WELTALLCORE_API TResult<std::vector<FVramImage>> ReadTimSequence(std::span<const uint8> Bytes);

	/**
	 * What the kernel's TIM archive load uploads from an archive (u32 count, then each TIM's offset),
	 * in its order: from the last TIM to the first, each one's CLUT (when it has one), then its pixels.
	 */
	WELTALLCORE_API TResult<std::vector<FVramImage>> ReadTimArchive(std::span<const uint8> Bytes);
}
