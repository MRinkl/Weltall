#pragma once

#include "Foundation/Types.h"
#include "Graphics/AffineBlend.h"
#include "Graphics/GteMatrix.h"
#include "WorldMap/WorldMapCameraOffsets.h"

#include <array>
#include <optional>

namespace Weltall
{
	struct FFieldViewInput;
	struct FWorldMapContext;

	/**
	 * The camera orbit's eye: where a camera Distance (20.12) from a target at height TargetY stands under a
	 * pitch and a yaw, x and z from the target and y in the world, in whole units.
	 */
	WELTALLCORE_API std::array<int16, 3> WorldMapOrbitEye(int16 TargetY, int32 Distance, int16 Pitch, int16 Yaw);

	/** The camera's matrix from its angles and the orbit's eye. */
	WELTALLCORE_API FGteMatrix WorldMapCameraMatrix(const std::array<int16, 3>& Eye, int16 Pitch, int16 Yaw, int16 Roll);

	/** The game's camera on its orbit as a tick leaves it: what the player's offsets are laid over. */
	struct FWorldMapCameraBasis
	{
		/** The target's height (whole units) and the eye's distance from it (20.12). */
		int16 TargetY = 0;
		int32 Distance = 0;
		int16 Pitch = 0;
		int16 Yaw = 0;
		int16 Roll = 0;

		bool operator==(const FWorldMapCameraBasis&) const = default;
	};

	/**
	 * The game's camera where the player's offsets can be laid over it: none in a cutscene, while a script's look-at
	 * has the camera, or while its eye stands off the orbit its angles and distance give.
	 */
	WELTALLCORE_API std::optional<FWorldMapCameraBasis> WorldMapCameraBasis(const FWorldMapContext& Context);

	/** The view a world map frame is drawn with: the game's camera moved by the player's offsets. */
	struct FWorldMapPlayerView
	{
		FGteMatrix Camera;
		/** The eye as the orbit keeps it. */
		std::array<int16, 3> Eye{};
		int16 Pitch = 0;
		int16 Yaw = 0;
		/** How much farther from the target the eye stands than the game's (whole units): what the drawing cuts and fogs by depth moves back with it. */
		int32 EyeBack = 0;
		/** The offsets as the view shows them, in the camera's whole steps. */
		FWorldMapCameraOffsets Shown;
	};

	/**
	 * The game's camera under the player's offsets, built as the game builds its own: the yaw turned, the pitch
	 * steeper by the tilt (to 85 degrees at most, or the game's own if steeper), the distance scaled. Default-made
	 * offsets give the game's camera, bit for bit.
	 */
	WELTALLCORE_API FWorldMapPlayerView WorldMapPlayerView(const FWorldMapCameraBasis& Basis, const FWorldMapCameraOffsets& Offsets);

	/** The same camera without the game's whole steps: what a display between ticks moves the view by. */
	WELTALLCORE_API FAffine WorldMapPlayerViewAffine(const FWorldMapCameraBasis& Basis, const FWorldMapCameraOffsets& Offsets);

	/** That camera as the sky is drawn under it: turned back by its yaw, so the sky's quads keep out of the turn. */
	WELTALLCORE_API FAffine WorldMapPlayerSkyAffine(const FWorldMapCameraBasis& Basis, const FWorldMapCameraOffsets& Offsets);

	/** The view's turn as the camera's yaw counts it (4096 a turn): the controls and the sprites' facing follow it. */
	WELTALLCORE_API int16 WorldMapViewTurn(const FWorldMapCameraOffsets& Offsets);

	/** The offsets after the player's input: turned, tilted and zoomed within their ranges, or put back as the game frames the map. */
	WELTALLCORE_API FWorldMapCameraOffsets MoveWorldMapCameraOffsets(const FWorldMapCameraOffsets& Offsets, const FFieldViewInput& Input);
}
