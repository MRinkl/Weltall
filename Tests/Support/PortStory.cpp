#include "Support/PortStory.h"

#include <catch_amalgamated.hpp>

namespace Weltall::Testing
{
	const FStoryPacks& PortStory()
	{
		static const TResult<FStoryPacks> Story = FStoryPacks::Read(WELTALL_STORY_DIR);
		INFO((Story ? std::string() : Story.Error().Message));
		REQUIRE(Story);
		return *Story;
	}
}
