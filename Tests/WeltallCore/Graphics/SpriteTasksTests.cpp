#include "Graphics/Sprite.h"
#include "Graphics/SpriteSystem.h"

#include <catch_amalgamated.hpp>

#include <algorithm>

using namespace Weltall;

namespace
{
	bool IsListed(const FSpriteSystem& System, const FSpriteTask& Task)
	{
		return std::any_of(System.Tasks().begin(), System.Tasks().end(), [&Task](const FKernelTaskEntry& Entry) { return Entry.Sprite.get() == &Task; });
	}
}

TEST_CASE("A retired task leaves the run while its sprite stays as it was for what still points at it", "[Graphics]")
{
	FSpriteSystem System;
	System.bCountingTasks = true;
	FSpriteTask& Retired = System.CreateTask();
	Retired.Sprite.Position = {0x120000, -0x40000, 0x3000000};
	FSpriteTask& Child = System.CreateTask();
	Child.Sprite.Parent = &Retired.Sprite;
	Child.Sprite.Target = &Retired.Sprite;
	REQUIRE(System.CountedTasks == 2);

	System.RetireTask(Retired);
	CHECK_FALSE(IsListed(System, Retired));
	CHECK(System.CountedTasks == 1);
	CHECK(Child.Sprite.Parent == &Retired.Sprite);
	CHECK(Child.Sprite.Target == &Retired.Sprite);
	CHECK(Retired.Sprite.Position == std::array<int32, 3>{0x120000, -0x40000, 0x3000000});

	// Its memory is not handed to the tasks made after it; a removed task's is, and what pointed at that one points at none.
	FSpriteTask& Made = System.CreateTask();
	CHECK(&Made != &Retired);
	FSpriteTask& Removed = System.CreateTask();
	Child.Sprite.Target = &Removed.Sprite;
	System.RemoveTask(Removed);
	CHECK(Child.Sprite.Target == nullptr);
	CHECK(&System.CreateTask() == &Removed);

	System.EndTasks();
	CHECK(System.Tasks().empty());
}
