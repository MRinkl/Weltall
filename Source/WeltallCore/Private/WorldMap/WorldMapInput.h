#pragma once

#include "WorldMap/WorldMapContext.h"

namespace Weltall
{
	/** What the pad asks of the task that reads it this tick. */
	enum class EWorldMapInput : uint8
	{
		None = 0,
		/** Confirm on a place: enter it (or touch a place entered by touch). */
		Enter = 1,
		/** Confirm while touching or near a collider. */
		Board = 3,
		/** The ship's Circle: disembark (or land). */
		Disembark = 4,
	};

	namespace WorldMapOps
	{
		/** The pad's logical buttons the world map reads. */
		namespace WorldMapButton
		{
			inline constexpr uint16 L2 = 0x0001;
			inline constexpr uint16 R2 = 0x0002;
			inline constexpr uint16 L1 = 0x0004;
			inline constexpr uint16 R1 = 0x0008;
			inline constexpr uint16 Menu = 0x0010;
			inline constexpr uint16 Confirm = 0x0020;
			inline constexpr uint16 Cancel = 0x0040;
			inline constexpr uint16 Throttle = 0x0080;
			inline constexpr uint16 Select = 0x0100;
			inline constexpr uint16 Start = 0x0800;
			inline constexpr uint16 Up = 0x1000;
			inline constexpr uint16 Right = 0x2000;
			inline constexpr uint16 Down = 0x4000;
			inline constexpr uint16 Left = 0x8000;
			inline constexpr uint16 Directions = 0xF000;
		}

		/** On foot: the heading from the d-pad and the camera's yaw, then confirm, the menu and L2 + R2. */
		EWorldMapInput ReadFootInput(FWorldMapContext& Context, FWorldMapTask& Task);
		/** The same in gears. */
		EWorldMapInput ReadGearInput(FWorldMapContext& Context, FWorldMapTask& Task);
		/** L2 and R2 newly held together. */
		void ReadGearToggle(FWorldMapContext& Context);
		/** The sailing Yggdrasil's heading from the d-pad, then confirm on a place, Circle and the menu. */
		EWorldMapInput ReadShipInput(FWorldMapContext& Context, FWorldMapTask& Task);
		/**
		 * The flying Yggdrasil's turn and bank (the d-pad's sides, L1, R1), its pitch
		 * (Up dives, Down climbs), its throttle (and R2 backwards), its direction; then confirm on a place,
		 * Circle when it is still and level, and the menu.
		 */
		EWorldMapInput ReadFlightInput(FWorldMapContext& Context, FWorldMapTask& Task);
	}
}
