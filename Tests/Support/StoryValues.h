#pragma once

#include "Field/EventScripts.h"
#include "Foundation/Types.h"

#include <set>
#include <vector>

namespace Weltall::Testing
{
	/** What a map's scripts make of the story's progress, and where they lead. */
	struct FStoryReferences
	{
		/** The values the scripts compare the story's progress with. */
		std::set<uint16> Compared;
		/** The maps the scripts leave for. */
		std::set<int32> Neighbours;
		/** The saved variables of the story's own (its flags and counts) the scripts test, each by its byte address. */
		std::set<uint16> Tested;
		/** The walkmesh's attribute records whose words the scripts change: the ways of the floor they open and close. */
		std::set<int32> WalkRecords;
	};

	/** Read from the instructions the scripts' threads reach. */
	FStoryReferences FindStoryReferences(const FEventScripts& Scripts);

	/**
	 * The values of the story's progress that scripts comparing it with Compared (at least one value) tell apart: each
	 * value itself, and one of each run of values between two of them, before the first and after the last.
	 */
	std::vector<uint16> StoryValuesToldApart(const std::set<uint16>& Compared);
}
