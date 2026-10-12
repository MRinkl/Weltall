#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Media/DiscIdentification.h"
#include "WorldMap/WorldMapDirectorTables.h"
#include "WorldMap/WorldMapScriptTables.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	/** A camera's or a model's path: points (x, y, z, w), the last with w -1. */
	using FWorldMapPath = std::vector<std::array<int16, 4>>;

	/** The cutscene scenes' own tables in the Worldmap program. */
	struct FWorldMapCutsceneTables
	{
		/** Scene 9's camera: offsets x and z from the craft (z as the drawing's), its height y. */
		FWorldMapPath Scene9Camera;
		/**
		 * Scene 12's camera about the fleet, then about the craft that overtakes it (offsets as scene 9's). Neither path
		 * ends in a point with w -1; the second is read one point past its seven, into the table after it.
		 */
		FWorldMapPath Scene12FleetCamera;
		FWorldMapPath Scene12LateShipCamera;
		/**
		 * Scene 14's flyer: places in the world. Nine points, then the three its last segments blend in from the
		 * tables after them.
		 */
		FWorldMapPath Scene14Flyer;
		/** The scripts and places of scenes 13, 15 and 16. */
		FWorldMapDirectorTables Directors;
		/** The scripts of scenes 17 and 18, and where scene 17's ribbons stop. */
		FWorldMapScriptTables Scripts;
	};

	/** Reads the cutscenes' tables from the unpacked Worldmap program of a release. */
	WELTALLCORE_API TResult<FWorldMapCutsceneTables> ReadWorldMapCutsceneTables(std::span<const uint8> Program, EDiscRelease Release);
}
