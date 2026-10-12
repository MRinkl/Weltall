#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Media/DiscIdentification.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	struct FFieldCamera;
	struct FGteArctangents;

	/**
	 * The screen markers the scripts show (FE D4): 33 small sprites, marker 0 of 16 x 16 pixels and the
	 * others of 8 x 8, each in its own colour. A script places a marker for the tick's frame only.
	 */
	struct FFieldScreenMarkers
	{
		static constexpr int32 Count = 33;
		/** Their texels in the effects' textures (a 4-bit page at (960, 256) with its CLUT at (256, 247)): marker 0's, then the others'. */
		static constexpr std::array<uint8, 2> BigTexels{0xE0, 0x70};
		static constexpr std::array<uint8, 2> SmallTexels{0xE0, 0x60};

		struct FPlaced
		{
			uint8 Marker = 0;
			/** The sprite's top left corner on the 320 x 224 screen. */
			int16 X = 0;
			int16 Y = 0;
		};

		/** Made and not given up: markers can be coloured and placed. */
		bool bMade = false;
		/** Each marker's colour, which modulates its texels (128 each: the texels' own). */
		std::array<std::array<uint8, 3>, Count> Colors{};
		/** The markers the tick's frame draws, in the order the scripts placed them. */
		std::vector<FPlaced> Placed;
	};

	/**
	 * The gear cockpit's instruments the scripts put up (FE B1) until the map is left: the pieces of
	 * one image (FFieldCockpitHudLayout), some scrolled each tick by the camera's heading and pitch and by
	 * the instruments' own counter.
	 */
	struct FFieldCockpitHud
	{
		/** The image's file in directory 4, and where it goes: a 4-bit page at (896, 0), its CLUT at (0, 232). */
		static constexpr int32 ImageFile = 0xAA;
		static constexpr int16 ImageX = 0x380;
		static constexpr int16 ImageY = 0;
		static constexpr int16 ClutY = 0xE8;

		bool bOn = false;
		/** Counts the ticks the instruments are drawn: the Field program's, from 0 when it starts and on from map to map. */
		uint32 Counter = 0;
		/** The blinking light's step, 0 to 2, one more every 16 ticks. */
		int32 Blink = 0;
		/** What the tick's frame showed: the camera's heading and pitch (4096 a turn), and the counter before its step. */
		int32 Heading = 0;
		int32 Pitch = 0;
		uint32 DrawnCounter = 0;

		/** Puts the instruments up: the blinking light starts over. */
		WELTALLCORE_API void Show();
		/**
		 * The tick's frame, when they are up: the heading is the view's direction the tick took before the
		 * camera moved, the pitch that of the camera's eye to its target as they are now.
		 */
		WELTALLCORE_API void Tick(const FFieldCamera& Camera, const FGteArctangents& Arctangents);
	};

	/** Where the instruments' pieces lie on the screen and in their image, as the Field program's tables hold them. */
	struct FFieldCockpitHudLayout
	{
		static constexpr int32 PieceCount = 109;

		struct FPiece
		{
			/** The top left corner on the 320 x 224 screen. */
			int16 X = 0;
			int16 Y = 0;
			/** The piece's rectangle of the image. */
			uint16 Image = 0;
			/** Bits 0-3: its texels as they are, mirrored across (1), upside down (2) or both (3); bits 4-7: 0 added, 1 subtracted. */
			uint16 Flags = 0;
		};

		struct FImage
		{
			uint16 U = 0;
			uint16 V = 0;
			uint16 Width = 0;
			uint16 Height = 0;
		};

		std::array<FPiece, PieceCount> Pieces{};
		std::vector<FImage> Images;

		/** Reads them from the Field program of a disc of the given release. */
		WELTALLCORE_API static TResult<FFieldCockpitHudLayout> Read(const FCdImage& Image, const FGameFileIndex& Index, EDiscRelease Release);
		/** Reads them from a release's Field program, unpacked. */
		WELTALLCORE_API static TResult<FFieldCockpitHudLayout> FromProgram(std::span<const uint8> Program, EDiscRelease Release);
	};

	/** One of the instruments' pieces as a frame draws it: a semi-transparent quad textured from their image. */
	struct FFieldHudQuad
	{
		bool bDrawn = false;
		/** Top left, top right, bottom left, bottom right: screen x and y, and their texels in the image's page. */
		std::array<std::array<int16, 2>, 4> Corners{};
		std::array<std::array<uint8, 2>, 4> Texels{};
		/** The GPU's blend: 0 half and half, 1 added, 2 subtracted. */
		uint8 BlendMode = 0;
	};

	/** The instruments' pieces as the tick's frame draws them, in the layout's order. */
	WELTALLCORE_API void BuildCockpitHudQuads(const FFieldCockpitHudLayout& Layout, const FFieldCockpitHud& Hud,
		std::span<FFieldHudQuad, FFieldCockpitHudLayout::PieceCount> Quads);
}
