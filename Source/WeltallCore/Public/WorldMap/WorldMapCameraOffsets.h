#pragma once

namespace Weltall
{
	/**
	 * The player's own framing of the world map, on top of the game's camera: how far the eye is pulled back (a factor
	 * of the game's distance to the target), how many degrees higher it looks down from, and how many degrees it is
	 * turned about the target (the way R1 turns the camera). A default-made one is the game's own view, which the
	 * view's reset returns to.
	 */
	struct FWorldMapCameraOffsets
	{
		static constexpr float MinZoom = 0.5f;
		static constexpr float MaxZoom = 2.0f;
		static constexpr float MinTilt = -10.0f;
		static constexpr float MaxTilt = 20.0f;

		float Zoom = 1.0f;
		float Tilt = 0.0f;
		float Turn = 0.0f;

		bool operator==(const FWorldMapCameraOffsets&) const = default;
	};
}
