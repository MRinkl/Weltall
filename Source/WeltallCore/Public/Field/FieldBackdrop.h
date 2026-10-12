#pragma once

#include "Field/FieldCamera.h"
#include "Field/FieldRenderState.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"

#include <array>
#include <vector>

namespace Weltall
{
	struct FGteArctangents;

	/** A packet of the panorama as the GPU takes it: a four-point polygon, flat, graded or textured raw. */
	struct FFieldBackdropQuad
	{
		enum class EKind : uint8
		{
			Flat,
			Graded,
			Textured,
		};

		EKind Kind = EKind::Flat;
		/** Pixels of the 320 x 240 screen the panorama assumes, in the GPU's corner order (0-1-2, then 1-3-2). */
		std::array<std::array<int16, 2>, 4> Corners{};
		/** A flat quad's colour is the first; a graded one's each corner's. */
		std::array<std::array<uint8, 3>, 4> Colors{};
		/** A textured quad's texels in its page, as the packet holds them: its last column and row are the far edge's. */
		std::array<std::array<uint8, 2>, 4> Texels{};
		uint16 TexturePage = 0;
		uint16 Clut = 0;
	};

	/**
	 * The map's panorama as a tick draws it: the backdrop the map made after its init (FE 80-FE 82),
	 * and where the camera puts it on the screen. It lies in the background table's entry before the
	 * far bucket, drawn after the background's farther entries and before the rest.
	 */
	struct FFieldBackdrop
	{
		bool bShown = false;
		FFieldPanorama Panorama;
		/** The strip's texel column the view's heading turns it to. */
		int32 ScrollU = 0;
		/** The screen y of the horizon's point (the strip's lower edge and the graded band's top), and of the graded band's end. */
		int32 HorizonY = 0;
		int32 BandY = 0;
		/** How far the camera's distance shrinks the strip: from 0 (texel for pixel) to 256 (half). */
		int32 Zoom = 0;
		/** The background table's entry it is linked into. */
		int32 Entry = 0;
	};

	/**
	 * Where the kernel's panorama drawing puts a panorama, the camera looking from Eye to Target (whole units) under Gte
	 * (its transform the view, its projection distance and screen offset).
	 */
	WELTALLCORE_API FFieldBackdrop PlacePanorama(const FFieldPanorama& Made, const FGteVector& Eye, const FGteVector& Target, const FGte& Gte,
		const FGteArctangents& Arctangents);

	/**
	 * Where the view puts the panorama the map made (Made; none when not enabled) this tick, the eye
	 * and target the camera's (16.16), not shown while it is hidden.
	 */
	WELTALLCORE_API FFieldBackdrop PlaceFieldBackdrop(const FFieldPanorama& Made, bool bHidden, const FFieldPoint& Eye, const FFieldPoint& Target,
		const FFieldView& View, const FGteArctangents& Arctangents);

	/**
	 * Latest placed Alpha of the way from Before, its frame before: the strip turned the shorter way round, the horizon,
	 * the band and the zoom in between, each a whole step as the drawing takes them. Latest as it is when Before was not shown.
	 */
	WELTALLCORE_API FFieldBackdrop BlendFieldBackdrop(const FFieldBackdrop& Before, const FFieldBackdrop& Latest, float Alpha);

	/** The screen columns the panorama covers: the original's screen. */
	inline constexpr int32 FieldBackdropWidth = 320;

	/**
	 * The backdrop's packets in drawing order over the screen's columns [Left, Right): the colour bands
	 * below, across and above the horizon (when the panorama has them), then the strip's quads, the last
	 * linked first. Over [0,
	 * FieldBackdropWidth) they are the original's packets; a wider range continues the strip and the
	 * bands to either side, the strip only when it turns with the view (a strip fixed to the screen is
	 * a picture of it alone). Quads keeps its capacity from call to call.
	 */
	WELTALLCORE_API void DrawFieldBackdrop(const FFieldBackdrop& Backdrop, int32 Left, int32 Right, std::vector<FFieldBackdropQuad>& Quads);
}
