#pragma once

#include "Foundation/Types.h"
#include "Graphics/AffineBlend.h"
#include "WorldMap/WorldMapFrame.h"

#include <array>
#include <span>
#include <utility>
#include <vector>

namespace Weltall
{
	/** Rows (x, y, z, then the translation) taking a point of the latest tick's view space to where it is drawn. */
	using FWorldMapViewMove = FViewMove;

	/** How a display draws the latest world map frame Alpha of the way from the tick before it. */
	struct FWorldMapFrameBlend
	{
		/** For each anchor of the latest frame, where its polygons are drawn: the identity where it does not blend. */
		std::vector<FWorldMapViewMove> Moves;
		/** For each anchor of the latest frame, whether it was drawn from the tick before (else as the latest tick has it). */
		std::vector<uint8> Paired;
		float ProjectionDistance = 0.0f;
		float ScreenCenterY = 0.0f;
		/** Texels the horizon's band is moved across by, as it scrolls with the camera's yaw. */
		float HorizonScroll = 0.0f;
		/** Whether the tick before was drawn from: no cut lay between the two. */
		bool bBlended = false;
	};

	/**
	 * For each anchor of Later, the index of Earlier's anchor with the same key, or -1. Sorts in Scratch: neither
	 * allocates once both have grown to the frames' sizes.
	 */
	WELTALLCORE_API void MatchWorldMapAnchors(const FWorldMapFrame& Earlier, const FWorldMapFrame& Later, std::vector<int32>& Matches,
		std::vector<std::pair<uint32, int32>>& Scratch);

	/**
	 * Latest drawn Alpha of the way from Previous (the tick before, its anchors paired by Matches): each anchor's
	 * matrix blended column by column (direction and length) and in its translation, the world's about the target
	 * the latest frame draws it about, as is the projection. Nothing blends across a cut (frames not consecutive, the
	 * camera's target jumping or its view turning sharply), and an anchor does not where it jumped or flipped.
	 */
	WELTALLCORE_API void BlendWorldMapFrames(const FWorldMapFrame& Latest, const FWorldMapFrame* Previous, std::span<const int32> Matches, float Alpha,
		FWorldMapFrameBlend& Blend);

	/**
	 * Moves a blend of Latest and Previous from the camera each tick was drawn with, in the whole steps its integers
	 * take, to the same camera unrounded. The rounding turns a moving camera back and forth by a fraction of the
	 * original's pixel from tick to tick, which a display many times its size shows as the trees' and the sprites'
	 * pixels shimmering. The sky keeps its own. Nothing where the player's view does not apply (a cutscene's camera,
	 * a script's look-at), which keeps the game's camera whole.
	 */
	WELTALLCORE_API void UnroundWorldMapBlend(const FWorldMapFrame& Latest, const FWorldMapFrame* Previous, float Alpha, FWorldMapFrameBlend& Blend);

	/**
	 * Moves a blend of Latest and Previous on to the player's view as it stands now: the offsets the blend shows (the
	 * two frames' Alpha of the way where they blend, else the latest's) give way to Live over the latest tick's
	 * camera, so the player's turn and zoom show in the display frame they are made in. The sky's quads keep out of
	 * the turn, which scrolls the horizon's band instead. Nothing where the player's view does not apply.
	 */
	WELTALLCORE_API void MoveWorldMapBlendToView(const FWorldMapFrame& Latest, const FWorldMapFrame* Previous, float Alpha,
		const FWorldMapCameraOffsets& Live, FWorldMapFrameBlend& Blend);
}
