#pragma once

#include "Story/StoryPacks.h"

namespace Weltall::Testing
{
	/** The port's own story as the checkout's Content/Story holds it, read once per test process; the test fails where it cannot be read. */
	const FStoryPacks& PortStory();
}
