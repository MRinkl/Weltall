#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Text/MessageTable.h"
#include "WorldMap/WorldMapScene.h"
#include "WorldMap/WorldMapTables.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Weltall
{
	/**
	 * What the port's story adds to a world map's places, written as text:
	 *
	 *   world map 8                     the era whose scene it is added to, 0 to 8
	 *   message MassDriver              a place's name, after the scene's own names (MesText.h's text, between a
	 *   {                               line of '{' and a line of '}': one line)
	 *   Mass Driver
	 *   }
	 *   place on foot x 29880 z 16287 width 202 height 214 to map 732 entrance 0 name MassDriver
	 *   arrival 40 x 29981 z 16520      where leaving a field map for the world map by entrance 40 puts the party
	 *   dot 21                          the minimap shows its place 21 (0 to 23) with the scene's
	 *   vehicle away at story 253       while the story is at 253 (or 253-255), the party's ship is elsewhere
	 *
	 * A place is entered with the confirm button where the party stands in its rectangle (whole units, edges
	 * included), on foot, in gears or in the vehicle as its row says, and leads to a field map by one of its
	 * entrances; "on touch" before "to" enters it as soon as the party stands there, on foot only. Its name is a
	 * message of the text, or one of the scene's own by its number (#20). A dot is one of the minimap's own places,
	 * shown wherever the text is added, as the game's are once its scripts have shown them. While the ship is away the
	 * world map shows none and the party travels on foot or in its gears, the ship's place and flags in the game kept
	 * as they were for when it is back. A ';' begins a comment.
	 */
	class FWorldMapPatch
	{
	public:
		/** Reads the text; the error names the line. */
		WELTALLCORE_API static TResult<FWorldMapPatch> Parse(std::string_view Text);

		int32 Era() const { return EraNumber; }

		/**
		 * Adds the places, their names and the arrivals to Scene: an error where an arrival's number is one of the
		 * scene's own, a place's list or a name of the scene's own is not there, or the names cannot be added.
		 */
		WELTALLCORE_API std::optional<FError> Apply(FWorldMapScene& Scene) const;

		/** Whether the text keeps the party's ship away while the story is at Story. */
		WELTALLCORE_API bool KeepsVehicleAway(uint16 Story) const;

	private:
		struct FPlace
		{
			/** The trigger list: on foot, in gears, in the vehicle. */
			int32 List = 0;
			FWorldMapTrigger Record;
			/** A name the text gives, or none: Record's name is then the scene's own. */
			std::string Name;
		};

		int32 EraNumber = 0;
		std::vector<std::pair<std::string, FAddedMessage>> Names;
		std::vector<FPlace> Places;
		std::vector<FWorldMapEntrance> Arrivals;
		/** The minimap's places shown, a bit each. */
		uint32 Dots = 0;
		/** The story's values, first and last, at which the ship is away. */
		std::vector<std::pair<uint16, uint16>> VehicleAway;
	};
}
