#pragma once

#include "Field/FieldBackdrop.h"
#include "Field/FieldRenderState.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"

#include <array>
#include <optional>
#include <vector>

namespace Weltall
{
	class FBattleStageRecord;
	struct FGteArctangents;

	/**
	 * A stage's floor dome as its element makes it: a plane of 8 x 8 textured cells a little ahead of the eye, turned to
	 * the camera's heading, tilted with its pitch and scrolling, with the sky's and the ground's colour bands and a glow
	 * at the horizon.
	 */
	struct FBattleFloorDome
	{
		/** How far the grid's texels have scrolled across and along, in sixteenths, and how far they scroll a frame. */
		std::array<int16, 2> Scroll{};
		std::array<int16, 2> ScrollStep{};
		/** The side of the texture's tile (a power of two): the texture window's, whose halves the cells take in turn. */
		int16 Tile = 0;
		/** How far the grid lies above a quarter of the eye's height. */
		int16 Lift = 0;
		/** The horizon's point scaled by these (8.8) ends the glow above the horizon and the graded band below it. */
		int16 GlowScale = 0;
		int16 BandScale = 0;
		/** The grid's projection distance, and how far the camera's pitch tilts it. */
		int16 ProjectionDistance = 0;
		int16 TiltScale = 0;
		/** The horizon's point: its height and its distance ahead of the camera's target. */
		int16 HorizonHeight = 0;
		int16 HorizonDistance = 0;
		/** The grid's points, 9 x 9 around its middle, row by row. */
		std::array<FGteVector, 81> Points{};
		/** The sky's colour, the graded band's top and the floor's (the band's bottom). */
		std::array<std::array<uint8, 3>, 3> Colors{};
		/** The glow's top (the sky's colour turned round) and its bottom (black), which the tint layers recolour apart. */
		std::array<std::array<uint8, 3>, 2> Glow{};
		/** The cells' 4-bit page (blend mode 1) and CLUT, and where the tile lies in the page. */
		uint16 TexturePage = 0;
		uint16 Clut = 0;
		int16 WindowX = 0;
		int16 WindowY = 0;
	};

	/** A packet of the floor dome as the GPU takes it: a draw mode, or a quad of the panorama's kinds. */
	struct FBattleDomePacket
	{
		bool bDrawMode = false;
		/** A draw mode's page bits (its blend mode) and texture window within the page; a window 0 wide is the drawing environment's. */
		uint16 DrawPage = 0;
		std::array<int16, 4> Window{};
		FFieldBackdropQuad Quad;
		bool bSemiTransparent = false;
	};

	/**
	 * A stage's backdrop as the setup's stage build makes it from its elements: the first two panoramas (the second kind's
	 * with the colour bands of the stage's clear colours) and the first floor dome.
	 */
	struct FBattleStageBackdrop
	{
		std::array<std::optional<FFieldPanorama>, 2> Panoramas;
		std::optional<FBattleFloorDome> Dome;
	};

	WELTALLCORE_API FBattleStageBackdrop MakeBattleStageBackdrop(const FBattleStageRecord& Record);

	/**
	 * One frame of the backdrop as the stage's drawing draws it after the stage's model, into the ordering table's last entry:
	 * the dome's packets in drawing order (the dome is linked last, so drawn first), then each panorama, the second first.
	 */
	struct FBattleBackdropFrame
	{
		std::vector<FBattleDomePacket> Dome;
		std::array<FFieldBackdrop, 2> Panoramas{};
	};

	/**
	 * The backdrop drawn with the camera looking from Eye to Target under Gte (its transform the view, its screen offset and
	 * projection distance the battle's): the panoramas placed, then the dome, whose scroll steps.
	 */
	WELTALLCORE_API void DrawBattleStageBackdrop(FBattleStageBackdrop& Backdrop, const FGteVector& Eye, const FGteVector& Target, const FGte& Gte,
		const FGteArctangents& Arctangents, FBattleBackdropFrame& Frame);
}
