#pragma once

namespace Weltall
{
	/**
	 * The player's own framing of the field, on top of the game's camera: how far the eye is pulled
	 * back (a factor of the game's distance to the target), how many degrees higher it looks down
	 * from, and how many degrees it is turned about the target (the way R1 turns the camera). A
	 * default-made one is the game's own view; `Defaults` is the framing a player starts with.
	 */
	struct FFieldCameraOffsets
	{
		static constexpr float MinZoom = 0.5f;
		static constexpr float MaxZoom = 2.0f;
		static constexpr float MinTilt = -10.0f;
		static constexpr float MaxTilt = 20.0f;
		static constexpr float DefaultZoom = 1.25f;

		float Zoom = 1.0f;
		float Tilt = 0.0f;
		float Turn = 0.0f;
		/**
		 * The framing steps aside for every scene: whenever the player cannot move for more than a moment the view
		 * eases to the game's own, and back when they can. Off: only while a script moves the camera.
		 */
		bool bYieldsToScenes = true;
		/**
		 * The view keeps close to the party as it walks: across the ground it closes on where the game's camera
		 * is heading faster than the game's does, and all the way. Off: the game's own follow, which trails behind
		 * and rests wherever it comes within a few units.
		 */
		bool bSteadyFollow = false;

		/** The player's framing until they change it, and what the view's reset returns to. */
		static constexpr FFieldCameraOffsets Defaults() { return {DefaultZoom, 0.0f, 0.0f, true, true}; }

		bool operator==(const FFieldCameraOffsets&) const = default;
	};
	/** The player's offsets as a view shows them: the zoom (1 the game's), the tilt and the turn (degrees, the way R1 turns). */
	struct FFieldViewOffsets
	{
		float Zoom = 1.0f;
		float Tilt = 0.0f;
		float Turn = 0.0f;

		bool operator==(const FFieldViewOffsets&) const = default;
	};
}
