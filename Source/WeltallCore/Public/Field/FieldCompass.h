#pragma once

#include "Foundation/Types.h"
#include "Graphics/GteMatrix.h"

#include <array>

namespace Weltall
{
	struct FEventContext;

	/** The compass in the screen's lower right: the needle's turn, and whether the tick draws it. */
	struct FFieldCompass
	{
		/** The needle's yaw (4096 a turn): toward the followed actor's facing as the screen shows it. */
		int16 Needle = 0;
		/** The tick draws the compass: the scripts have not hidden it and the map has settled. */
		bool bDrawn = false;
	};

	/**
	 * The compass's pieces as the Field program keeps them: each quad's four corners (x, z on the
	 * compass's ground) and texels (top left, top right, bottom left, bottom right), the two textures
	 * (the dial's and the needle's with the letters') as the page's depth, place and CLUT, the letters'
	 * places, each direction's pair of camera direction bits, and the shadow's quads.
	 */
	struct FFieldCompassLayout
	{
		static constexpr int32 DialQuads = 16;
		static constexpr int32 Letters = 4;
		static constexpr int32 ShadowQuads = 4;
		static constexpr int32 Directions = 8;

		struct FQuad
		{
			std::array<std::array<int16, 2>, 4> Corners{};
			std::array<std::array<uint8, 2>, 4> Texels{};
		};

		struct FTexture
		{
			/** The GPU's texture depth: 0 4-bit, 1 8-bit. */
			int16 Depth = 0;
			int16 PageX = 0;
			int16 PageY = 0;
			int16 ClutX = 0;
			int16 ClutY = 0;
		};

		/** The dial's 4 x 4 quads, the letters' four, and the needle, textured by Textures[0], [1] and [1]. */
		std::array<FQuad, DialQuads> Dial{};
		std::array<FQuad, Letters> LetterQuads{};
		FQuad Needle;
		std::array<FTexture, 2> Textures{};
		/** Where each letter stands on the compass's ground (x, z). */
		std::array<std::array<int16, 2>, Letters> LetterPlaces{};
		/** A direction's sixteen colours of the dial's palette go black while the camera may turn to neither of its pair. */
		std::array<uint16, Directions> DirectionBits{};
		/** The shadow under the dial: four quads, semi-transparent and subtracted, from a 4-bit page. */
		std::array<FQuad, ShadowQuads> Shadow{};
		FTexture ShadowTexture;
	};

	/** A quad of the compass as the field links it: on the 320 x 224 screen, from its page and CLUT. */
	struct FFieldCompassQuad
	{
		std::array<std::array<int16, 2>, 4> Corners{};
		std::array<std::array<uint8, 2>, 4> Texels{};
		FFieldCompassLayout::FTexture Texture;
		bool bShadow = false;
	};

	/** The compass as a tick draws it: its quads in the order the GPU draws them. */
	struct FFieldCompassFrame
	{
		static constexpr int32 MaxQuads = FFieldCompassLayout::ShadowQuads + FFieldCompassLayout::DialQuads + FFieldCompassLayout::Letters + 1;

		std::array<FFieldCompassQuad, MaxQuads> Quads{};
		int32 Count = 0;
	};

	/**
	 * The compass's step at its place in the tick (after the fades' step): the needle turned 0x40 a
	 * tick toward the followed actor's facing plus the view's yaw and the player's turn (at once while
	 * the map settles), and whether the tick draws it.
	 */
	WELTALLCORE_API void TurnFieldCompass(FEventContext& Context);

	/**
	 * The compass as the field draws it on the 320 x 224 screen under a view: its own projection (H
	 * 0x80, centred at (266, 166)), the dial turned as Rotation (the look-at with the screen's rotation)
	 * turns the world and tipped as Pitch (the camera's pitch alone) tips the ground, the needle at its
	 * yaw on the tipped ground, the letters standing at their places, all of it only when the tick draws
	 * the compass.
	 */
	WELTALLCORE_API void DrawFieldCompass(const FFieldCompass& Compass, const FFieldCompassLayout& Layout, const FGteMatrix& Rotation, const FGteMatrix& Pitch,
		FFieldCompassFrame& Frame);

	/**
	 * The dial's palette as the field loads it each tick into its CLUT's row: the compass's sixteen
	 * colours for each of the eight directions, black for a direction whose pair the camera's
	 * forbidden directions (a bit each) touch.
	 */
	WELTALLCORE_API std::array<uint16, 16 * FFieldCompassLayout::Directions> FieldCompassPalette(const FFieldCompassLayout& Layout, const std::array<uint16, 16>& Colors,
		uint8 ForbiddenDirections);
}
